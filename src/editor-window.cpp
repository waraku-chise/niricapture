#include "editor-window.hpp"

#include "editor/editor-canvas.hpp"
#include "editor/tool-bar.hpp"
#include "i18n.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int kDefaultCanvasWidth = 1600;
constexpr int kDefaultCanvasHeight = 900;
constexpr int kEditorMargin = 48;

QString imageFilter() {
  return QObject::tr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif);;All files (*)");
}

QImage checkerTile(const QColor &dark, const QColor &light) {
  QImage tile(16, 16, QImage::Format_RGB32);
  tile.fill(dark);
  QPainter painter(&tile);
  painter.fillRect(QRect(0, 0, 8, 8), light);
  painter.fillRect(QRect(8, 8, 8, 8), light);
  return tile;
}

CaptureData captureForImage(const QImage &image) {
  CaptureData capture;
  capture.source = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  capture.previewSize = capture.source.size();
  capture.monitor.geometry = QRect(QPoint(0, 0), capture.source.size());
  capture.monitor.pixelSize = capture.source.size();
  capture.monitor.scale = 1.0;
  return capture;
}
} // namespace

class EditorWindow::CanvasView final : public QWidget {
public:
  explicit CanvasView(EditorWindow *owner, const AppConfig &config)
      : QWidget(owner), owner_(owner), config_(config),
        bar_(canvas_.registry()) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setAttribute(Qt::WA_OpaquePaintEvent);
    canvas_.registry().setVisibleTools(config_.tools);
    canvas_.setToolShortcuts(config_.shortcuts);
    EditorStyle style;
    style.color = config_.theme.defaultColor;
    style.width = config_.theme.defaultWidth;
    canvas_.setDefaultStyle(style);
    canvas_.setTool(canvas_.registry().defaultToolId());
    bar_.setCommands(config_.commands);
    bar_.setPalette(config_.palette.colors, config_.palette.widths);
    bar_.setTheme(config_.theme.panel, QColor(255, 255, 255, 30),
                  config_.theme.accent);
    canvas_.setStatusCallback(
        [this](const QString &text) { owner_->setStatus(text); });
    canvas_.setRepaintCallback([this] { update(); });
    canvas_.setTextCallback([this](const QPointF &point) { beginText(point); });
  }

  void setImage(const QImage &image) {
    if (image.isNull())
      return;
    capture_ = captureForImage(image);
    imageSize_ = capture_.previewSize;
    canvas_.document().clear();
    canvas_.setSelection(QRectF(QPointF(0, 0), QSizeF(imageSize_)));
    canvas_.setTool(QStringLiteral("select"));
    zoom_ = 1.0;
    fit_ = true;
    pan_ = {};
    computeFit();
    update();
  }

  [[nodiscard]] QImage image() const { return capture_.source; }
  [[nodiscard]] QSize imageSize() const { return imageSize_; }
  [[nodiscard]] int padding() const { return padding_; }
  [[nodiscard]] BackgroundStyle backgroundStyle() const { return background_; }
  [[nodiscard]] QColor backgroundColor() const { return backgroundColor_; }

  void setCanvasSize(const QSize &size) {
    if (size.isEmpty() || size == imageSize_)
      return;
    QImage resized(size, QImage::Format_ARGB32_Premultiplied);
    resized.fill(Qt::transparent);
    QPainter painter(&resized);
    painter.drawImage(QPoint(0, 0), capture_.source);
    painter.end();
    capture_.source = resized;
    capture_.previewSize = size;
    capture_.monitor.geometry = QRect(QPoint(0, 0), size);
    capture_.monitor.pixelSize = size;
    imageSize_ = size;
    canvas_.setSelection(QRectF(QPointF(0, 0), QSizeF(size)));
    fit_ = true;
    computeFit();
    update();
  }

  void setPadding(int padding) {
    padding_ = std::clamp(padding, 0, 512);
    fit_ = true;
    computeFit();
    update();
  }

  void setBackground(BackgroundStyle style, const QColor &color) {
    background_ = style;
    backgroundColor_ = color;
    update();
  }

  void zoomIn() {
    fit_ = false;
    zoom_ = std::clamp(zoom_ * 1.25, 0.05, 8.0);
    owner_->updateZoomLabel(zoom_);
    update();
  }

  void zoomOut() {
    fit_ = false;
    zoom_ = std::clamp(zoom_ / 1.25, 0.05, 8.0);
    owner_->updateZoomLabel(zoom_);
    update();
  }

  void fitToWindow() {
    fit_ = true;
    computeFit();
    update();
  }

  [[nodiscard]] QImage renderContent() const {
    if (capture_.source.isNull())
      return {};
    return renderCapture(capture_, QRectF(QPointF(0, 0), QSizeF(imageSize_)),
                         canvas_.document().annotations(),
                         BackgroundStyle::None);
  }

  [[nodiscard]] QImage renderOutput() const {
    QImage content = renderContent();
    if (content.isNull())
      return {};
    if (padding_ == 0 && background_ == BackgroundStyle::None &&
        backgroundColor_.alpha() == 0)
      return content;
    QImage output(content.width() + padding_ * 2,
                  content.height() + padding_ * 2,
                  QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::transparent);
    QPainter painter(&output);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);
    const QRectF canvasRect(QPointF(0, 0), QSizeF(output.size()));
    if (background_ != BackgroundStyle::None) {
      paintCaptureBackground(painter, canvasRect, background_);
    } else if (backgroundColor_.alpha() > 0) {
      painter.fillRect(canvasRect, backgroundColor_);
    }
    painter.drawImage(QPoint(padding_, padding_), content);
    painter.end();
    return output;
  }

  void beginTextAt(const QPointF &point) {
    if (textEditor_)
      commitText();
    textEditor_ = new QLineEdit(this);
    textPoint_ = point;
    textEditor_->setStyleSheet(
        QStringLiteral("QLineEdit { color: %1; background: rgba(16,18,24,225); "
                       "border: 1px solid rgba(255,255,255,45); "
                       "border-radius: 5px; padding: 2px 4px; }")
            .arg(canvas_.style().color.name()));
    textEditor_->setFont(annotationTextFont(5.0));
    textEditor_->resize(240, 34);
    const QRectF image = imageRect();
    const QPointF local = image.topLeft() + point * zoom_;
    textEditor_->move(
        std::clamp(qRound(local.x()), 8,
                   std::max(8, width() - 248)),
        std::clamp(qRound(local.y() - 17), 8,
                   std::max(8, height() - 42)));
    textEditor_->show();
    textEditor_->setFocus(Qt::ActiveWindowFocusReason);
    connect(textEditor_, &QLineEdit::editingFinished, this,
            &CanvasView::commitText);
    owner_->setStatus(ui("Text · Enter to commit", "文字 · Enter 提交"));
  }

  void clear() {
    if (textEditor_)
      commitText();
    canvas_.document().clear();
    capture_ = {};
    imageSize_ = {};
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.fillRect(rect(), config_.theme.background);
    if (capture_.source.isNull())
      return;
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform |
                           QPainter::TextAntialiasing);
    const QRectF canvas = canvasRect();
    const QRectF image = imageRect();

    painter.save();
    painter.setClipRect(canvas);
    painter.fillRect(canvas,
                     QBrush(QPixmap::fromImage(
                         checkerTile(config_.theme.checkerDark,
                                     config_.theme.checkerLight))));
    if (background_ != BackgroundStyle::None) {
      paintCaptureBackground(painter, canvas, background_);
    } else if (backgroundColor_.alpha() > 0) {
      painter.fillRect(canvas, backgroundColor_);
    }
    painter.restore();

    painter.drawImage(image, capture_.source);

    painter.save();
    painter.setClipRect(image);
    painter.translate(image.topLeft());
    painter.scale(zoom_, zoom_);
    canvas_.paintVectors(painter, QPointF(0, 0));
    painter.restore();

    syncToolbar();
    bar_.paint(painter, rect(), image);
  }

  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    if (fit_)
      computeFit();
  }

  void mousePressEvent(QMouseEvent *event) override {
    setFocus(Qt::MouseFocusReason);
    const QPointF local = event->position();
    const ToolBarHit hit = bar_.hitTest(rect(), imageRect(), local);
    if (hit.valid) {
      handleToolbarHit(hit);
      event->accept();
      return;
    }
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && spaceDown_)) {
      panning_ = true;
      panStart_ = local;
      panOrigin_ = pan_;
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }
    if (event->button() != Qt::LeftButton)
      return;
    if (!imageRect().contains(local))
      return;
    setCursor(Qt::CrossCursor);
    dragging_ = true;
    canvas_.setSelection(QRectF(QPointF(0, 0), QSizeF(imageSize_)));
    ToolEvent toolEvent;
    toolEvent.pos = toCanvas(local);
    toolEvent.local = toolEvent.pos;
    toolEvent.modifiers = event->modifiers();
    canvas_.press(toolEvent);
    update();
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    const QPointF local = event->position();
    if (panning_) {
      pan_ = panOrigin_ + (local - panStart_);
      fit_ = false;
      update();
      event->accept();
      return;
    }
    if (dragging_) {
      ToolEvent toolEvent;
      toolEvent.pos = toCanvas(local);
      toolEvent.local = toolEvent.pos;
      toolEvent.modifiers = event->modifiers();
      canvas_.move(toolEvent);
      update();
      event->accept();
      return;
    }
    if (imageRect().contains(local))
      setCursor(spaceDown_ ? Qt::OpenHandCursor : Qt::CrossCursor);
    else
      setCursor(Qt::ArrowCursor);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (panning_) {
      panning_ = false;
      setCursor(Qt::ArrowCursor);
      event->accept();
      return;
    }
    if (dragging_) {
      ToolEvent toolEvent;
      toolEvent.pos = toCanvas(event->position());
      toolEvent.local = toolEvent.pos;
      canvas_.release(toolEvent);
      dragging_ = false;
      update();
      event->accept();
    }
  }

  void wheelEvent(QWheelEvent *event) override {
    if (event->modifiers().testFlag(Qt::ControlModifier) ||
        event->modifiers().testFlag(Qt::MetaModifier)) {
      const qreal oldZoom = zoom_;
      const qreal steps = event->angleDelta().y() / 120.0;
      if (qFuzzyIsNull(steps)) {
        event->ignore();
        return;
      }
      fit_ = false;
      const QRectF oldImage = imageRect();
      const QPointF cursor = event->position();
      const QPointF canvasPoint =
          (cursor - oldImage.topLeft()) / oldZoom;
      zoom_ = std::clamp(zoom_ * (steps > 0 ? 1.15 : 1.0 / 1.15), 0.05, 8.0);
      const QPointF anchored = cursor - canvasPoint * zoom_;
      pan_ += anchored - imageRect().topLeft();
      owner_->updateZoomLabel(zoom_);
      update();
      event->accept();
      return;
    }
    if (canvas_.wheel(event)) {
      update();
      event->accept();
    } else {
      event->ignore();
    }
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
      spaceDown_ = true;
      setCursor(Qt::OpenHandCursor);
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Escape) {
      if (textEditor_) {
        textEditor_->hide();
        textEditor_->deleteLater();
        textEditor_ = nullptr;
      } else if (canvas_.document().selected() >= 0) {
        canvas_.document().setSelected(-1);
        update();
      } else if (canvas_.currentToolId() != QStringLiteral("select")) {
        canvas_.setTool(QStringLiteral("select"));
        update();
      } else {
        owner_->showHome();
      }
      event->accept();
      return;
    }
    if (event->matches(QKeySequence::Copy)) {
      owner_->copyImage();
      event->accept();
      return;
    }
    if (event->matches(QKeySequence::Save)) {
      owner_->saveImage();
      event->accept();
      return;
    }
    if (canvas_.keyPress(event)) {
      update();
      event->accept();
      return;
    }
    QWidget::keyPressEvent(event);
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
      spaceDown_ = false;
      setCursor(Qt::ArrowCursor);
      event->accept();
      return;
    }
    QWidget::keyReleaseEvent(event);
  }

private:
  void computeFit() {
    if (imageSize_.isEmpty() || width() <= 0 || height() <= 0)
      return;
    const QSizeF content = QSizeF(imageSize_) +
                           QSizeF(padding_ * 2, padding_ * 2);
    const qreal availableWidth = std::max(1, width() - kEditorMargin * 2);
    const qreal availableHeight = std::max(1, height() - kEditorMargin * 2);
    zoom_ = std::clamp(
        std::min(availableWidth / content.width(),
                 availableHeight / content.height()),
        0.02, 4.0);
    pan_ = {};
    owner_->updateZoomLabel(zoom_);
  }

  [[nodiscard]] QRectF canvasRect() const {
    const QSizeF content = QSizeF(imageSize_) +
                           QSizeF(padding_ * 2, padding_ * 2);
    const QSizeF shown = content * zoom_;
    const QPointF origin((width() - shown.width()) / 2.0 + pan_.x(),
                         (height() - shown.height()) / 2.0 + pan_.y());
    return {origin, shown};
  }

  [[nodiscard]] QRectF imageRect() const {
    const QRectF canvas = canvasRect();
    return QRectF(canvas.topLeft() + QPointF(padding_, padding_) * zoom_,
                  QSizeF(imageSize_) * zoom_);
  }

  [[nodiscard]] QPointF toCanvas(const QPointF &local) const {
    const QRectF image = imageRect();
    return (local - image.topLeft()) / std::max<qreal>(zoom_, 0.001);
  }

  void handleToolbarHit(const ToolBarHit &hit) {
    if (!hit.valid)
      return;
    if (hit.isTool) {
      canvas_.setTool(hit.id);
    } else if (hit.id == QStringLiteral("palette")) {
      bar_.setPaletteOpen(!bar_.paletteOpen());
    } else if (hit.id == QStringLiteral("undo")) {
      canvas_.document().undo();
    } else if (hit.id == QStringLiteral("redo")) {
      canvas_.document().redo();
    } else if (hit.id == QStringLiteral("ocr")) {
      owner_->runOcr();
    } else if (hit.id == QStringLiteral("copy")) {
      owner_->copyImage();
    } else if (hit.id == QStringLiteral("save")) {
      owner_->saveImage();
    } else if (hit.id == QStringLiteral("close")) {
      owner_->showHome();
    } else if (hit.id.startsWith(QStringLiteral("color-"))) {
      const int index = hit.id.sliced(6).toInt();
      const QColor color = bar_.colorAt(index);
      if (color.isValid()) {
        canvas_.style().color = color;
        if (canvas_.document().selected() >= 0) {
          canvas_.document().pushUndo();
          canvas_.document().annotations()[canvas_.document().selected()]
              .color = canvas_.style().color;
        }
      }
    } else if (hit.id.startsWith(QStringLiteral("size-"))) {
      canvas_.style().width = std::clamp(hit.id.sliced(5).toInt(), 2, 64);
      if (canvas_.document().selected() >= 0) {
        canvas_.document().pushUndo();
        canvas_.document().annotations()[canvas_.document().selected()]
            .size = canvas_.style().width;
      }
    } else if (hit.id == QStringLiteral("fill")) {
      canvas_.style().filled = !canvas_.style().filled;
      if (canvas_.document().selected() >= 0) {
        canvas_.document().pushUndo();
        canvas_.document().annotations()[canvas_.document().selected()]
            .filled = canvas_.style().filled;
      }
    }
    update();
  }

  void syncToolbar() {
    bar_.setCurrentTool(canvas_.currentToolId());
    bar_.setColors(canvas_.style().color, canvas_.style().filled);
    bar_.setHistory(canvas_.document().canUndo(),
                    canvas_.document().canRedo());
  }

  void beginText(const QPointF &point) {
    beginTextAt(point);
  }

  void commitText() {
    if (!textEditor_)
      return;
    const QString text = textEditor_->text().trimmed();
    if (!text.isEmpty()) {
      canvas_.document().pushUndo();
      Annotation annotation;
      annotation.kind = Annotation::Kind::Text;
      annotation.start = textPoint_;
      annotation.start.ry() +=
          QFontMetricsF(annotationTextFont(5.0)).ascent();
      annotation.text = text;
      annotation.color = canvas_.style().color;
      annotation.size = 5.0;
      canvas_.document().add(std::move(annotation));
    }
    textEditor_->hide();
    textEditor_->deleteLater();
    textEditor_ = nullptr;
    update();
  }

  EditorWindow *owner_;
  AppConfig config_;
  EditorCanvas canvas_;
  ToolBar bar_;
  CaptureData capture_;
  QSize imageSize_;
  qreal zoom_ = 1.0;
  bool fit_ = true;
  QPointF pan_;
  int padding_ = 0;
  BackgroundStyle background_ = BackgroundStyle::None;
  QColor backgroundColor_ = Qt::transparent;
  bool spaceDown_ = false;
  bool panning_ = false;
  bool dragging_ = false;
  QPointF panStart_;
  QPointF panOrigin_;
  QLineEdit *textEditor_ = nullptr;
  QPointF textPoint_;
};

EditorWindow::EditorWindow(const AppConfig &config, QWidget *parent)
    : QWidget(parent), config_(config) {
  setWindowTitle(QStringLiteral("NiriCapture"));
  resize(1200, 820);
  setMinimumSize(760, 520);
  QString style = QStringLiteral(R"(
    QWidget { background: #0f1116; color: #eef1f7; }
    QLabel { background: transparent; }
    QLabel#title { font-size: 34px; font-weight: 700; color: #ffffff; }
    QLabel#subtitle { font-size: 14px; color: #9aa3b2; }
    QFrame#homeCard { background: #151922; border: 1px solid #262c38;
                      border-radius: 18px; }
    QPushButton { background: #202633; color: #eef1f7; border: 1px solid #303849;
                  border-radius: 8px; padding: 7px 12px; }
    QPushButton:hover { background: #2a3343; }
    QPushButton:pressed { background: #18202c; }
    QPushButton#primary { background: #2f6fed; border-color: #4b83ff;
                          font-weight: 600; padding: 12px 22px; font-size: 15px; }
    QPushButton#primary:hover { background: #3f7df7; }
    QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {
      background: #171c25; color: #eef1f7; border: 1px solid #303849;
      border-radius: 7px; padding: 5px 7px; }
    QLabel#status { color: #9aa3b2; padding: 5px 12px; }
    QLabel#zoom { color: #cfd6e2; min-width: 52px; }
  )");
  style.replace(QStringLiteral("#0f1116"), config_.theme.background.name());
  style.replace(QStringLiteral("#151922"), config_.theme.panel.name());
  style.replace(QStringLiteral("#2f6fed"), config_.theme.accent.name());
  style.replace(QStringLiteral("#3f7df7"), config_.theme.accent.lighter(115).name());
  setStyleSheet(style);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  stack_ = new QStackedWidget(this);
  layout->addWidget(stack_);
  buildHome();
  buildEditor();
  stack_->setCurrentWidget(home_);

  connect(&ocrWatcher_, &QFutureWatcher<OcrJob>::finished, this, [this] {
    ocrRunning_ = false;
    const OcrJob job = ocrWatcher_.result();
    if (!job.error.isEmpty()) {
      setStatus(job.error);
      return;
    }
    showOcrDialog(job.text);
  });
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape && stack_->currentWidget() == home_) {
    close();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

void EditorWindow::buildHome() {
  home_ = new QWidget(stack_);
  home_->setObjectName(QStringLiteral("home"));
  auto *outer = new QVBoxLayout(home_);
  outer->setContentsMargins(80, 64, 80, 64);
  outer->addStretch(1);

  auto *card = new QFrame(home_);
  card->setObjectName(QStringLiteral("homeCard"));
  card->setMinimumWidth(620);
  card->setMaximumWidth(760);
  auto *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(52, 46, 52, 46);
  cardLayout->setSpacing(14);

  auto *title = new QLabel(QStringLiteral("NiriCapture"), card);
  title->setObjectName(QStringLiteral("title"));
  auto *subtitle = new QLabel(
      ui("A quiet screenshot editor for Niri. Start with a blank canvas or "
         "open an image.",
         "为 Niri 打造的轻量截图编辑器。新建画布，或打开一张图片开始。"),
      card);
  subtitle->setObjectName(QStringLiteral("subtitle"));
  subtitle->setWordWrap(true);
  cardLayout->addWidget(title);
  cardLayout->addWidget(subtitle);
  cardLayout->addSpacing(18);

  auto *buttons = new QHBoxLayout;
  buttons->setSpacing(12);
  auto *newButton = new QPushButton(ui("New canvas", "新建画布"), card);
  newButton->setObjectName(QStringLiteral("primary"));
  newButton->setCursor(Qt::PointingHandCursor);
  auto *openButton = new QPushButton(ui("Open image", "打开图片"), card);
  openButton->setObjectName(QStringLiteral("primary"));
  openButton->setCursor(Qt::PointingHandCursor);
  buttons->addWidget(newButton);
  buttons->addWidget(openButton);
  buttons->addStretch(1);
  cardLayout->addLayout(buttons);

  auto *hint = new QLabel(
      ui("Tip: Ctrl+F1 captures a window. The screenshot tools stay available "
         "from the launcher.",
         "提示：Ctrl+F1 截取窗口；也可以从启动器打开这个编辑器处理图片。"),
      card);
  hint->setObjectName(QStringLiteral("subtitle"));
  hint->setWordWrap(true);
  cardLayout->addSpacing(18);
  cardLayout->addWidget(hint);

  outer->addWidget(card, 0, Qt::AlignHCenter);
  outer->addStretch(1);
  stack_->addWidget(home_);

  connect(newButton, &QPushButton::clicked, this, &EditorWindow::newFromDialog);
  connect(openButton, &QPushButton::clicked, this, &EditorWindow::openFromDialog);

  auto *quit = new QPushButton(ui("Quit", "退出"), home_);
  quit->setCursor(Qt::PointingHandCursor);
  outer->addWidget(quit, 0, Qt::AlignHCenter);
  connect(quit, &QPushButton::clicked, this, &QWidget::close);
}

void EditorWindow::buildEditor() {
  editorPage_ = new QWidget(stack_);
  auto *layout = new QVBoxLayout(editorPage_);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto *bar = new QWidget(editorPage_);
  bar->setStyleSheet(QStringLiteral(
      "QWidget#topbar { background: #151922; border-bottom: 1px solid #262c38; }"));
  bar->setObjectName(QStringLiteral("topbar"));
  auto *barLayout = new QHBoxLayout(bar);
  barLayout->setContentsMargins(12, 8, 12, 8);
  barLayout->setSpacing(6);

  auto addButton = [&](const QString &text, auto handler) {
    auto *button = new QPushButton(text, bar);
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this, handler);
    barLayout->addWidget(button);
    return button;
  };

  addButton(ui("Home", "首页"), [this] { showHome(); });
  addButton(ui("Open", "打开"), [this] { openFromDialog(); });
  addButton(ui("Save", "保存"), [this] { saveImage(); });
  addButton(ui("Copy", "复制"), [this] { copyImage(); });
  addButton(ui("Pin", "钉图"), [this] { pinImage(); });
  barLayout->addSpacing(12);

  auto *zoomOut = new QPushButton(QStringLiteral("−"), bar);
  zoomOut->setToolTip(ui("Zoom out", "缩小"));
  auto *zoomIn = new QPushButton(QStringLiteral("+"), bar);
  zoomIn->setToolTip(ui("Zoom in", "放大"));
  auto *fit = new QPushButton(ui("Fit", "适应"), bar);
  zoomLabel_ = new QLabel(QStringLiteral("100%"), bar);
  zoomLabel_->setObjectName(QStringLiteral("zoom"));
  zoomLabel_->setAlignment(Qt::AlignCenter);
  barLayout->addWidget(zoomOut);
  barLayout->addWidget(zoomLabel_);
  barLayout->addWidget(zoomIn);
  barLayout->addWidget(fit);
  connect(zoomOut, &QPushButton::clicked, this, [this] {
    if (view_)
      view_->zoomOut();
  });
  connect(zoomIn, &QPushButton::clicked, this, [this] {
    if (view_)
      view_->zoomIn();
  });
  connect(fit, &QPushButton::clicked, this, [this] {
    if (view_)
      view_->fitToWindow();
  });
  barLayout->addStretch(1);
  addButton(ui("Canvas", "画布"), [this] { canvasFromDialog(); });
  addButton(ui("Close", "关闭"), [this] { close(); });
  layout->addWidget(bar);

  view_ = new CanvasView(this, config_);
  layout->addWidget(view_, 1);
  status_ = new QLabel(editorPage_);
  status_->setObjectName(QStringLiteral("status"));
  status_->setText(ui("Ready", "就绪"));
  layout->addWidget(status_);
  stack_->addWidget(editorPage_);
}

void EditorWindow::showHome() {
  stack_->setCurrentWidget(home_);
  setWindowTitle(QStringLiteral("NiriCapture"));
}

void EditorWindow::showCanvas() {
  stack_->setCurrentWidget(editorPage_);
  if (view_)
    view_->setFocus(Qt::OtherFocusReason);
}

void EditorWindow::openImage(const QImage &image) {
  if (image.isNull())
    return;
  if (!view_)
    return;
  view_->setImage(image);
  view_->setPadding(config_.canvas.padding);
  view_->setBackground(config_.backgroundStyle(), config_.canvas.backgroundColor);
  showCanvas();
  setStatus(ui("Image opened · %1 × %2", "已打开图片 · %1 × %2")
                .arg(image.width())
                .arg(image.height()));
}

void EditorWindow::newCanvas(const QSize &size) {
  if (size.isEmpty())
    return;
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  openImage(image);
  setStatus(ui("New canvas · %1 × %2 · choose a background in Canvas",
               "新建画布 · %1 × %2 · 可在“画布”中选择背景")
                .arg(size.width())
                .arg(size.height()));
}

void EditorWindow::openFromDialog() {
  const QString path = QFileDialog::getOpenFileName(
      this, ui("Open image", "打开图片"), lastImageDirectory_, imageFilter());
  if (path.isEmpty())
    return;
  lastImageDirectory_ = QFileInfo(path).absolutePath();
  QImageReader reader(path);
  reader.setAutoTransform(true);
  const QImage image = reader.read();
  if (image.isNull()) {
    QMessageBox::warning(this, ui("Open failed", "打开失败"),
                         reader.errorString());
    return;
  }
  openImage(image);
}

void EditorWindow::newFromDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle(ui("New canvas", "新建画布"));
  auto *layout = new QFormLayout(&dialog);
  auto *width = new QSpinBox(&dialog);
  width->setRange(1, 20000);
  width->setValue(config_.canvas.width);
  auto *height = new QSpinBox(&dialog);
  height->setRange(1, 20000);
  height->setValue(config_.canvas.height);
  layout->addRow(ui("Width", "宽度"), width);
  layout->addRow(ui("Height", "高度"), height);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                           QDialogButtonBox::Cancel,
                                       &dialog);
  layout->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() == QDialog::Accepted)
    newCanvas(QSize(width->value(), height->value()));
}

void EditorWindow::canvasFromDialog() {
  if (!view_)
    return;
  QDialog dialog(this);
  dialog.setWindowTitle(ui("Canvas", "画布"));
  auto *layout = new QFormLayout(&dialog);
  auto *width = new QSpinBox(&dialog);
  width->setRange(1, 20000);
  width->setValue(view_->imageSize().width());
  auto *height = new QSpinBox(&dialog);
  height->setRange(1, 20000);
  height->setValue(view_->imageSize().height());
  auto *padding = new QSpinBox(&dialog);
  padding->setRange(0, 512);
  padding->setValue(view_->padding());
  auto *background = new QComboBox(&dialog);
  background->addItem(ui("Transparent", "透明"), 0);
  background->addItem(ui("White", "白色"), 1);
  background->addItem(ui("Dark", "深色"), 2);
  background->addItem(QStringLiteral("Aurora"), 3);
  background->addItem(QStringLiteral("Sunset"), 4);
  background->addItem(QStringLiteral("Lagoon"), 5);
  background->addItem(QStringLiteral("Violet"), 6);
  if (view_->backgroundStyle() == BackgroundStyle::Aurora)
    background->setCurrentIndex(3);
  else if (view_->backgroundStyle() == BackgroundStyle::Sunset)
    background->setCurrentIndex(4);
  else if (view_->backgroundStyle() == BackgroundStyle::Lagoon)
    background->setCurrentIndex(5);
  else if (view_->backgroundStyle() == BackgroundStyle::Violet)
    background->setCurrentIndex(6);
  else if (view_->backgroundColor() == Qt::white)
    background->setCurrentIndex(1);
  else if (view_->backgroundColor() == QColor(QStringLiteral("#151922")))
    background->setCurrentIndex(2);
  layout->addRow(ui("Width", "宽度"), width);
  layout->addRow(ui("Height", "高度"), height);
  layout->addRow(ui("Padding", "内边距"), padding);
  layout->addRow(ui("Background", "背景"), background);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                           QDialogButtonBox::Cancel,
                                       &dialog);
  layout->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() != QDialog::Accepted)
    return;
  view_->setCanvasSize(QSize(width->value(), height->value()));
  view_->setPadding(padding->value());
  switch (background->currentData().toInt()) {
  case 1:
    view_->setBackground(BackgroundStyle::None, Qt::white);
    break;
  case 2:
    view_->setBackground(BackgroundStyle::None,
                         QColor(QStringLiteral("#151922")));
    break;
  case 3:
    view_->setBackground(BackgroundStyle::Aurora, Qt::transparent);
    break;
  case 4:
    view_->setBackground(BackgroundStyle::Sunset, Qt::transparent);
    break;
  case 5:
    view_->setBackground(BackgroundStyle::Lagoon, Qt::transparent);
    break;
  case 6:
    view_->setBackground(BackgroundStyle::Violet, Qt::transparent);
    break;
  default:
    view_->setBackground(BackgroundStyle::None, Qt::transparent);
    break;
  }
  setStatus(ui("Canvas updated", "画布已更新"));
}

void EditorWindow::saveImage() {
  if (!view_)
    return;
  const QImage output = view_->renderOutput();
  if (output.isNull()) {
    setStatus(ui("Nothing to save", "没有可保存的内容"));
    return;
  }
  QString start = lastImageDirectory_;
  if (start.isEmpty())
    start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if (start.isEmpty())
    start = QDir::homePath();
  QString path = QFileDialog::getSaveFileName(
      this, ui("Save image", "保存图片"),
      QDir(start).filePath(QStringLiteral("niricapture.png")),
      QStringLiteral("PNG (*.png)"));
  if (path.isEmpty())
    return;
  if (!path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
    path += QStringLiteral(".png");
  lastImageDirectory_ = QFileInfo(path).absolutePath();
  if (!output.save(path, "PNG")) {
    setStatus(ui("Could not save image", "无法保存图片"));
    return;
  }
  setStatus(ui("Saved %1", "已保存 %1").arg(path));
}

void EditorWindow::copyImage() {
  if (!view_)
    return;
  const QImage output = view_->renderOutput();
  QString error;
  if (output.isNull() || !quickOutput(output, QuickOutputMode::Copy, error)) {
    setStatus(error.isEmpty() ? ui("Could not copy image", "无法复制图片")
                              : error);
    return;
  }
  setStatus(ui("Copied to clipboard", "已复制到剪贴板"));
}

void EditorWindow::pinImage() {
  if (!view_)
    return;
  const QImage output = view_->renderOutput();
  if (output.isNull())
    return;
  ++pinCount_;
  prunePinnedSnapshots();
  const QString path = pinnedSnapshotPath(pinCount_);
  QString error;
  if (path.isEmpty() || !saveTemporarySnapshot(output, path, error)) {
    --pinCount_;
    setStatus(error.isEmpty() ? ui("Could not pin image", "无法钉图") : error);
    return;
  }
  if (!QProcess::startDetached(QCoreApplication::applicationFilePath(),
                               {QStringLiteral("--pin"), path})) {
    --pinCount_;
    QFile::remove(path);
    setStatus(ui("Could not start pinned image", "无法启动钉图"));
    return;
  }
  setStatus(ui("Pinned", "已钉图"));
}

void EditorWindow::runOcr() {
  if (!view_ || ocrRunning_)
    return;
  const QImage content = view_->renderContent();
  if (content.isNull())
    return;
  ocrRunning_ = true;
  setStatus(ui("Reading text…", "正在识别文字…"));
  ocrWatcher_.setFuture(QtConcurrent::run([content] {
    OcrJob job;
    job.text = recognizeText(content, job.error);
    return job;
  }));
}

void EditorWindow::showOcrDialog(const QString &text) {
  if (text.isEmpty())
    return;
  auto *dialog = new QDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(ui("OCR result", "OCR 结果"));
  dialog->resize(560, 420);
  auto *layout = new QVBoxLayout(dialog);
  auto *edit = new QPlainTextEdit(text, dialog);
  edit->setReadOnly(true);
  layout->addWidget(edit, 1);
  auto *buttons = new QHBoxLayout;
  auto *copy = new QPushButton(ui("Copy text", "复制文本"), dialog);
  auto *close = new QPushButton(ui("Close", "关闭"), dialog);
  buttons->addWidget(copy);
  buttons->addWidget(close);
  buttons->addStretch(1);
  layout->addLayout(buttons);
  connect(copy, &QPushButton::clicked, dialog, [this, edit] {
    QString error;
    if (!copyTextToClipboard(edit->toPlainText(), error))
      setStatus(error);
    else
      setStatus(ui("OCR text copied", "OCR 文本已复制"));
  });
  connect(close, &QPushButton::clicked, dialog, &QDialog::close);
  dialog->show();
  setStatus(ui("OCR ready", "OCR 完成"));
}

void EditorWindow::setStatus(const QString &text) {
  if (status_)
    status_->setText(text);
}

void EditorWindow::updateZoomLabel(qreal zoom) {
  if (zoomLabel_)
    zoomLabel_->setText(QStringLiteral("%1%").arg(qRound(zoom * 100.0)));
}
