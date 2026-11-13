/** @fileoverview Frozen-frame selection overlay with an in-place toolbar. */
#include "region-selector.hpp"

#include "editor/annotation-geometry.hpp"
#include "editor/font-picker.hpp"
#include "i18n.hpp"
#include "md3.hpp"

#include <LayerShellQt/Window>

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QColorDialog>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr qreal kMinSelection = 4.0;
constexpr qreal kHandleSize = 10.0;
constexpr qreal kHandleHit = 16.0;
constexpr int kDimAlpha = 120;

const QColor kPanel(17, 19, 24, 246);
const QColor kPanelBorder(255, 255, 255, 30);

QString handleAt(const QRectF &selection, const QPointF &point) {
  if (selection.isEmpty())
    return {};
  const qreal x0 = selection.left();
  const qreal x1 = selection.right();
  const qreal y0 = selection.top();
  const qreal y1 = selection.bottom();
  const qreal cx = selection.center().x();
  const qreal cy = selection.center().y();
  struct Handle {
    const char *name;
    QPointF point;
  };
  const std::array<Handle, 8> handles{{
      {"tl", {x0, y0}}, {"t", {cx, y0}}, {"tr", {x1, y0}},
      {"r", {x1, cy}},  {"br", {x1, y1}}, {"b", {cx, y1}},
      {"bl", {x0, y1}}, {"l", {x0, cy}},
  }};
  for (const Handle &handle : handles) {
    if (QRectF(handle.point.x() - kHandleHit / 2.0,
               handle.point.y() - kHandleHit / 2.0, kHandleHit, kHandleHit)
            .contains(point))
      return QString::fromLatin1(handle.name);
  }
  return {};
}

QRectF normalizedRect(const QPointF &a, const QPointF &b) {
  return QRectF(QPointF(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                QSizeF(std::abs(b.x() - a.x()), std::abs(b.y() - a.y())));
}

void drawPill(QPainter &painter, const QRectF &rect, const QString &text) {
  painter.setPen(QPen(kPanelBorder, 1));
  painter.setBrush(kPanel);
  painter.drawRoundedRect(rect, 9, 9);
  painter.setPen(QColor(245, 247, 250));
  painter.drawText(rect, Qt::AlignCenter, text);
}
} // namespace

/** A full-output layer surface showing one frozen frame. */
class RegionOverlay final : public QWidget {
public:
  RegionOverlay(RegionSelector *selector, const OutputFrame *frame,
                QWidget *parent = nullptr)
      : QWidget(parent), selector_(selector), frame_(frame) {
    setWindowTitle(QStringLiteral("NiriCapture selection"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
  }

  [[nodiscard]] QString outputName() const { return frame_->monitor.name; }
  [[nodiscard]] const OutputFrame *frame() const { return frame_; }

  void configureLayer() {
    QScreen *target = nullptr;
    for (QScreen *screen : QGuiApplication::screens()) {
      if (screen->name() == frame_->monitor.name) {
        target = screen;
        break;
      }
    }
    if (!target && QGuiApplication::primaryScreen())
      target = QGuiApplication::primaryScreen();
    if (target) {
      setScreen(target);
      setGeometry(target->geometry());
    }
    winId();
    QWindow *window = windowHandle();
    LayerShellQt::Window *layer =
        window ? LayerShellQt::Window::get(window) : nullptr;
    if (!layer) {
      qCritical() << "Could not create layer-shell surface for"
                  << frame_->monitor.name;
      return;
    }
    layer->setScreen(target);
    layer->setScope(QStringLiteral("niricapture-region"));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    LayerShellQt::Window::Anchors anchors;
    anchors.setFlag(LayerShellQt::Window::AnchorTop);
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorLeft);
    anchors.setFlag(LayerShellQt::Window::AnchorRight);
    layer->setAnchors(anchors);
    layer->setExclusiveZone(-1);
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    layer->setActivateOnShow(true);
  }

  void setKeyboardOwner(bool owner) {
    QWindow *window = windowHandle();
    LayerShellQt::Window *layer =
        window ? LayerShellQt::Window::get(window) : nullptr;
    if (!layer)
      return;
    layer->setKeyboardInteractivity(
        owner ? LayerShellQt::Window::KeyboardInteractivityExclusive
              : LayerShellQt::Window::KeyboardInteractivityNone);
    if (owner) {
      activateWindow();
      setFocus(Qt::ActiveWindowFocusReason);
    }
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform |
                           QPainter::TextAntialiasing);
    if (!frame_->image.isNull())
      painter.drawImage(rect(), frame_->image);

    const QRectF globalSelection = selector_->selection();
    const QPoint origin = frame_->monitor.geometry.topLeft();
    const QRectF local = globalSelection.translated(-origin);
    const QRectF visible =
        local.intersected(QRectF(QPointF(0, 0), QSizeF(size())));
    const bool hasSelection =
        !globalSelection.isEmpty() && !visible.isEmpty();

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, kDimAlpha));
    if (!hasSelection) {
      painter.drawRect(rect());
    } else {
      const int left = std::max(0, qFloor(visible.left()));
      const int top = std::max(0, qFloor(visible.top()));
      const int right = std::min(width(), qCeil(visible.right()));
      const int bottom = std::min(height(), qCeil(visible.bottom()));
      painter.drawRect(QRect(0, 0, width(), top));
      painter.drawRect(QRect(0, bottom, width(), height() - bottom));
      painter.drawRect(QRect(0, top, left, bottom - top));
      painter.drawRect(QRect(right, top, width() - right, bottom - top));
    }

    selector_->paintEditor(painter, this);
    if (!selector_->hasSelection()) {
      QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
      font.setPixelSize(12);
      painter.setFont(font);
      const QString hint = ui("Drag to select a region  ·  Esc to cancel",
                              "拖动选择区域 · Esc 取消");
      const qreal width = QFontMetricsF(font).horizontalAdvance(hint) + 28;
      drawPill(painter, QRectF((this->width() - width) / 2.0,
                               this->height() - 46, width, 30),
               hint);
    }
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (selector_->editorKey(event, this))
      return;
    QWidget::keyPressEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    selector_->setKeyboardOverlay(this);
    const QPointF local = event->position();
    const QPointF global = local + QPointF(frame_->monitor.geometry.topLeft());
    if (event->button() == Qt::RightButton) {
      selector_->editorRightClick();
      event->accept();
      return;
    }
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    ToolBarHit hit;
    if (selector_->toolbarHit(this, local, &hit)) {
      selector_->handleToolbarHit(hit);
      event->accept();
      return;
    }
    if (selector_->editorPress(this, global, local, event->button(),
                               event->modifiers()))
      event->accept();
    else
      QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    const QPointF local = event->position();
    const QPointF global = local + QPointF(frame_->monitor.geometry.topLeft());
    selector_->editorMove(this, global, event->modifiers());
    selector_->setToolbarHover(this, local);
    if (!(event->buttons() & Qt::LeftButton) &&
        selector_->toolbarContains(this, local))
      setCursor(Qt::PointingHandCursor);
    else if (!selector_->hasSelection())
      setCursor(Qt::CrossCursor);
    else if (!handleAt(selector_->selection(), global).isEmpty())
      setCursor(Qt::SizeAllCursor);
    else
      setCursor(Qt::CrossCursor);
    event->accept();
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() != Qt::LeftButton) {
      QWidget::mouseReleaseEvent(event);
      return;
    }
    const QPointF global =
        event->position() + QPointF(frame_->monitor.geometry.topLeft());
    selector_->editorRelease(this, global, event->modifiers());
    event->accept();
  }

  void wheelEvent(QWheelEvent *event) override {
    if (selector_->editorWheel(event))
      event->accept();
    else
      event->ignore();
  }

private:
  RegionSelector *selector_;
  const OutputFrame *frame_;
};

RegionSelector::RegionSelector(DesktopFrames frames, QuickOutputMode quickMode,
                               const AppConfig &config, QObject *parent)
    : QObject(parent),
      frames_(std::move(frames)),
      quickMode_(quickMode),
      toolBar_(canvas_.registry()) {
  canvas_.registry().setVisibleTools(config.tools);
  canvas_.setToolShortcuts(config.shortcuts);
  canvas_.setWidthOptions(config.palette.widths);
  EditorStyle style;
  style.color = config.theme.defaultColor;
  style.width = config.theme.defaultWidth;
  style.fontFamily = config.theme.fontFamily;
  canvas_.setDefaultStyle(style);
  canvas_.setTool(canvas_.registry().defaultToolId());
  toolBar_.setCommands(config.commands);
  toolBar_.setPalette(config.palette.colors, config.palette.widths);
  toolBar_.setTheme(md3Tokens(config.theme));
  canvas_.setColorSampler([this](const QPointF &point) {
    for (const OutputFrame &frame : frames_.outputs) {
      if (!QRectF(frame.monitor.geometry).contains(point) ||
          frame.image.isNull())
        continue;
      const qreal sx = frame.image.width() /
                       static_cast<qreal>(std::max(1,
                                                   frame.monitor.geometry.width()));
      const qreal sy =
          frame.image.height() /
          static_cast<qreal>(std::max(1, frame.monitor.geometry.height()));
      const QPoint pixel(
          qFloor((point.x() - frame.monitor.geometry.left()) * sx),
          qFloor((point.y() - frame.monitor.geometry.top()) * sy));
      if (frame.image.rect().contains(pixel))
        return frame.image.pixelColor(pixel);
    }
    return QColor();
  });
  canvas_.setStatusCallback([this](const QString &status) { setStatus(status); });
  canvas_.setRepaintCallback([this] { selectionChanged(); });
  canvas_.setTextCallback([this](const QPointF &point) {
    beginText(point, ownerOverlay());
  });
  connect(&ocrWatcher_, &QFutureWatcher<OcrJob>::finished, this, [this] {
    const OcrJob result = ocrWatcher_.result();
    if (!result.error.isEmpty()) {
      setStatus(result.error);
      return;
    }
    showOcrPanel(result.text, ocrOverlay_);
  });
}

RegionSelector::~RegionSelector() {
  closeOcrPanel();
  if (textEditor_)
    textEditor_->deleteLater();
  for (RegionOverlay *overlay : overlays_)
    delete overlay;
}

bool RegionSelector::hasSelection() const {
  return selection_.width() >= kMinSelection &&
         selection_.height() >= kMinSelection;
}

qreal RegionSelector::selectedScale() const {
  return scaleForSelection(frames_, selection_);
}

RegionOverlay *RegionSelector::ownerOverlay() const {
  for (RegionOverlay *overlay : overlays_) {
    if (overlay->outputName() == ownerOutput_)
      return overlay;
  }
  return overlays_.isEmpty() ? nullptr : overlays_.first();
}

void RegionSelector::start(const QRectF &initialSelection) {
  if (started_ || frames_.outputs.isEmpty())
    return;
  started_ = true;
  QString focused;
  QString focusError;
  if (!probeFocusedOutputName(focused, focusError))
    focused = frames_.outputs.first().monitor.name;
  ownerOutput_ = focused;
  keyboardOutput_ = focused;

  for (const OutputFrame &frame : frames_.outputs) {
    auto *overlay = new RegionOverlay(this, &frame);
    overlay->configureLayer();
    overlays_.push_back(overlay);
  }
  for (RegionOverlay *overlay : overlays_)
    overlay->show();
  for (RegionOverlay *overlay : overlays_)
    overlay->setKeyboardOwner(overlay->outputName() == focused);
  if (!initialSelection.isEmpty()) {
    selection_ = initialSelection.normalized();
    editing_ = true;
    updateOwnerOutput();
    canvas_.setSelection(selection_);
    if (quickMode_ != QuickOutputMode::None)
      QTimer::singleShot(0, this, [this] { exportSelection(quickMode_); });
  }
  selectionChanged();

  const QString testSelection =
      qEnvironmentVariable("NIRICAPTURE_TEST_SELECT").trimmed();
  if (!testSelection.isEmpty()) {
    const QStringList parts = testSelection.split(QLatin1Char(','));
    if (parts.size() == 4) {
      selection_ = QRectF(parts.at(0).toDouble(), parts.at(1).toDouble(),
                          parts.at(2).toDouble(), parts.at(3).toDouble());
      editing_ = true;
      updateOwnerOutput();
      canvas_.setSelection(selection_);
      const int delay =
          qEnvironmentVariableIntValue("NIRICAPTURE_TEST_SELECT_DELAY");
      QTimer::singleShot(delay > 0 ? delay : 250, this, [this] {
        selectionChanged();
        if (qEnvironmentVariableIsSet("NIRICAPTURE_TEST_DRAW")) {
          const QPointF base = selection_.topLeft();
          canvas_.setTool(QStringLiteral("rectangle"));
          ToolEvent press;
          press.pos = base + QPointF(80, 80);
          ToolEvent move = press;
          move.pos = base + QPointF(300, 240);
          canvas_.press(press);
          canvas_.move(move);
          canvas_.release(move);
          canvas_.setTool(QStringLiteral("arrow"));
          press.pos = base + QPointF(360, 120);
          move = press;
          move.pos = base + QPointF(520, 240);
          canvas_.press(press);
          canvas_.move(move);
          canvas_.release(move);
          canvas_.setTool(QStringLiteral("marker"));
          press.pos = base + QPointF(600, 120);
          canvas_.press(press);
          canvas_.release(press);
          canvas_.setTool(QStringLiteral("mosaic"));
          press.pos = base + QPointF(700, 300);
          move.pos = base + QPointF(820, 420);
          canvas_.press(press);
          canvas_.move(move);
          canvas_.release(move);
          selectionChanged();
          if (qEnvironmentVariableIsSet("NIRICAPTURE_TEST_EXPORT"))
            exportSelection(QuickOutputMode::Save);
        }
        if (qEnvironmentVariableIsSet("NIRICAPTURE_TEST_OCR"))
          runOcr();
        if (!qEnvironmentVariableIsSet("NIRICAPTURE_TEST_DRAW") &&
            !qEnvironmentVariableIsSet("NIRICAPTURE_TEST_EXPORT") &&
            quickMode_ != QuickOutputMode::None)
          exportSelection(quickMode_);
      });
    }
  }
  if (qEnvironmentVariableIsSet("NIRICAPTURE_TEST_GEOMETRY")) {
    QTimer::singleShot(500, this, [this] {
      beginSelection(QPointF(350, 180));
      editorMove(ownerOverlay(), QPointF(1450, 880), Qt::NoModifier);
      editorRelease(ownerOverlay(), QPointF(1450, 880), Qt::NoModifier);
      if (ownerOverlay()) {
        dragOriginalSelection_ = selection_;
        drag_ = Drag::ResizeSelection;
        resizeHandle_ = QStringLiteral("br");
        editorMove(ownerOverlay(), QPointF(1700, 1000), Qt::NoModifier);
        editorRelease(ownerOverlay(), QPointF(1700, 1000), Qt::NoModifier);
      }
      selectionChanged();
    });
  }
  if (qEnvironmentVariableIsSet("NIRICAPTURE_TEST_CANCEL"))
    QTimer::singleShot(200, this, &RegionSelector::cancel);
}

QPointF clampToBounds(const QPointF &point, const QRectF &bounds) {
  return {std::clamp(point.x(), bounds.left(), bounds.right()),
          std::clamp(point.y(), bounds.top(), bounds.bottom())};
}

QRectF clampSelectionToBounds(const QRectF &rect, const QRectF &bounds) {
  QRectF result = rect;
  if (result.width() > bounds.width())
    result.setWidth(bounds.width());
  if (result.height() > bounds.height())
    result.setHeight(bounds.height());
  result.moveLeft(
      std::clamp(result.left(), bounds.left(), bounds.right() - result.width()));
  result.moveTop(
      std::clamp(result.top(), bounds.top(), bounds.bottom() - result.height()));
  return result;
}

void RegionSelector::selectionChanged() {
  canvas_.setSelection(selection_);
  for (RegionOverlay *overlay : overlays_)
    overlay->update();
}

void RegionSelector::setStatus(const QString &status) {
  status_ = status;
  selectionChanged();
}

void RegionSelector::beginSelection(const QPointF &global) {
  canvas_.document().clear();
  selection_ = QRectF(clampPoint(global), QSizeF(0, 0));
  dragStart_ = global;
  drag_ = Drag::NewSelection;
  editing_ = false;
  selectionChanged();
}

void RegionSelector::moveSelectionBy(const QPointF &delta) {
  if (!hasSelection())
    return;
  selection_ = clampSelection(selection_.translated(delta));
  selectionChanged();
}

void RegionSelector::resizeSelectionTo(const QString &handle,
                                       const QPointF &global) {
  QRectF rect = dragOriginalSelection_;
  qreal left = rect.left();
  qreal top = rect.top();
  qreal right = rect.right();
  qreal bottom = rect.bottom();
  if (handle.contains(QLatin1Char('l')))
    left = std::min(global.x(), right - kMinSelection);
  if (handle.contains(QLatin1Char('r')))
    right = std::max(global.x(), left + kMinSelection);
  if (handle.contains(QLatin1Char('t')))
    top = std::min(global.y(), bottom - kMinSelection);
  if (handle.contains(QLatin1Char('b')))
    bottom = std::max(global.y(), top + kMinSelection);
  selection_ = clampSelection(
      QRectF(QPointF(left, top), QPointF(right, bottom)).normalized());
  selectionChanged();
}

void RegionSelector::finishSelection() {
  if (!hasSelection()) {
    editing_ = false;
    selectionChanged();
    return;
  }
  editing_ = true;
  updateOwnerOutput();
  canvas_.setSelection(selection_);
  if (quickMode_ != QuickOutputMode::None) {
    exportSelection(quickMode_);
    return;
  }
  setStatus(ui("Drag handles to resize  ·  choose a tool to annotate",
               "拖动控制点调整大小 · 选择工具开始标注"));
}

void RegionSelector::updateOwnerOutput() {
  if (frames_.outputs.isEmpty())
    return;
  qreal bestArea = -1.0;
  QString best = frames_.outputs.first().monitor.name;
  for (const OutputFrame &frame : frames_.outputs) {
    const QRectF intersection =
        selection_.intersected(QRectF(frame.monitor.geometry));
    const qreal area = intersection.isEmpty()
                           ? 0.0
                           : intersection.width() * intersection.height();
    if (area > bestArea) {
      bestArea = area;
      best = frame.monitor.name;
    }
  }
  ownerOutput_ = best;
}

void RegionSelector::setKeyboardOverlay(RegionOverlay *overlay) {
  keyboardOutput_ = overlay->outputName();
  for (RegionOverlay *candidate : overlays_)
    candidate->setKeyboardOwner(candidate == overlay);
}

QPointF RegionSelector::clampPoint(const QPointF &point) const {
  return clampToBounds(point, frames_.logicalBounds);
}

QRectF RegionSelector::clampSelection(const QRectF &rect) const {
  return clampSelectionToBounds(rect, frames_.logicalBounds);
}

void RegionSelector::cancel() {
  if (finished_)
    return;
  finished_ = true;
  closeOcrPanel();
  for (RegionOverlay *overlay : overlays_)
    overlay->hide();
  emit cancelled();
}

void RegionSelector::closeAll() {
  if (finished_)
    return;
  finished_ = true;
  closeOcrPanel();
  if (textEditor_)
    textEditor_->hide();
  for (RegionOverlay *overlay : overlays_)
    overlay->hide();
}

void RegionSelector::syncToolbar() {
  toolBar_.setCurrentTool(canvas_.currentToolId());
  toolBar_.setColors(canvas_.style().color, canvas_.style().filled);
  toolBar_.setWidth(canvas_.style().width);
  toolBar_.setStrokeStyle(canvas_.style().strokeStyle);
  toolBar_.setHistory(canvas_.document().canUndo(),
                      canvas_.document().canRedo());
}

bool RegionSelector::toolbarHit(RegionOverlay *overlay, const QPointF &local,
                                ToolBarHit *hit) {
  if (overlay->outputName() != ownerOutput_ || !hasSelection())
    return false;
  const QPoint origin = overlay->frame()->monitor.geometry.topLeft();
  const QRectF selection = selection_.translated(-origin);
  const ToolBarHit result =
      toolBar_.hitTest(QRectF(0, 0, overlay->width(), overlay->height()),
                       selection, local);
  if (hit)
    *hit = result;
  return result.valid;
}

bool RegionSelector::toolbarContains(RegionOverlay *overlay,
                                     const QPointF &local) const {
  if (overlay->outputName() != ownerOutput_ || !hasSelection())
    return false;
  const QPoint origin = overlay->frame()->monitor.geometry.topLeft();
  const QRectF selection = selection_.translated(-origin);
  return toolBar_.contains(QRectF(0, 0, overlay->width(), overlay->height()),
                           selection, local);
}

void RegionSelector::setToolbarHover(RegionOverlay *overlay,
                                     const QPointF &local) {
  if (!hasSelection() || overlay->outputName() != ownerOutput_) {
    toolBar_.setHovered({});
    return;
  }
  const QPoint origin = overlay->frame()->monitor.geometry.topLeft();
  const QRectF selection = selection_.translated(-origin);
  const ToolBarHit hit = toolBar_.hitTest(
      QRectF(0, 0, overlay->width(), overlay->height()), selection, local);
  toolBar_.setHovered(hit.valid ? hit.id : QString());
  overlay->update();
}

void RegionSelector::chooseFont() {
  QWidget *parent = overlays_.isEmpty() ? nullptr : overlays_.first();
  if (!parent)
    return;
  if (fontPicker_) {
    fontPicker_->deleteLater();
    fontPicker_ = nullptr;
  }
  fontPicker_ = new FontPicker(
      canvas_.style().fontFamily, parent, [this](const QString &family) {
        fontPicker_ = nullptr;
        canvas_.style().fontFamily = family;
        if (canvas_.document().selected() >= 0) {
          Annotation &annotation =
              canvas_.document().annotations()[canvas_.document().selected()];
          if (annotation.kind == Annotation::Kind::Text) {
            canvas_.document().pushUndo();
            annotation.fontFamily = family;
          }
        }
        selectionChanged();
      });
  fontPicker_->showCentered();
}

void RegionSelector::handleToolbarHit(const ToolBarHit &hit) {
  if (!hit.valid)
    return;
  if (hit.isTool) {
    canvas_.setTool(hit.id);
  } else if (hit.id == QStringLiteral("palette")) {
    toolBar_.setPaletteOpen(!toolBar_.paletteOpen());
  } else if (hit.id == QStringLiteral("font")) {
    chooseFont();
  } else if (hit.id == QStringLiteral("undo")) {
    canvas_.document().undo();
  } else if (hit.id == QStringLiteral("redo")) {
    canvas_.document().redo();
  } else if (hit.id == QStringLiteral("ocr")) {
    runOcr();
  } else if (hit.id == QStringLiteral("copy")) {
    exportSelection(QuickOutputMode::Copy);
  } else if (hit.id == QStringLiteral("save")) {
    exportSelection(QuickOutputMode::Save);
  } else if (hit.id == QStringLiteral("close")) {
    cancel();
  } else if (hit.id.startsWith(QStringLiteral("color-"))) {
    const int index = hit.id.sliced(6).toInt();
    const QColor color = toolBar_.colorAt(index);
    if (color.isValid()) {
      canvas_.style().color = color;
      if (canvas_.document().selected() >= 0) {
        canvas_.document().pushUndo();
        Annotation &selected = canvas_.document().annotations()[
            canvas_.document().selected()];
        selected.color = canvas_.style().color;
      }
    }
  } else if (hit.id == QStringLiteral("line-style")) {
    canvas_.style().strokeStyle =
        nextStrokeStyle(canvas_.style().strokeStyle);
    if (canvas_.document().selected() >= 0) {
      canvas_.document().pushUndo();
      canvas_.document().annotations()[canvas_.document().selected()]
          .strokeStyle = canvas_.style().strokeStyle;
    }
  } else if (hit.id.startsWith(QStringLiteral("size-"))) {
    canvas_.style().width =
        std::clamp(hit.id.sliced(5).toInt(), 1, 96);
    if (canvas_.document().selected() >= 0) {
      canvas_.document().pushUndo();
      Annotation &selected = canvas_.document().annotations()[
          canvas_.document().selected()];
      selected.size = canvas_.style().width;
    }
  } else if (hit.id == QStringLiteral("custom-color")) {
    QWidget *parent = overlays_.isEmpty() ? nullptr : overlays_.first();
    const QColor color = QColorDialog::getColor(
        canvas_.style().color, parent, ui("Custom colour", "自定义颜色"),
        QColorDialog::DontUseNativeDialog);
    if (color.isValid()) {
      canvas_.style().color = color;
      if (canvas_.document().selected() >= 0) {
        canvas_.document().pushUndo();
        Annotation &selected = canvas_.document().annotations()[
            canvas_.document().selected()];
        selected.color = color;
      }
    }
  } else if (hit.id == QStringLiteral("fill")) {
    canvas_.style().filled = !canvas_.style().filled;
    if (canvas_.document().selected() >= 0) {
      canvas_.document().pushUndo();
      Annotation &selected = canvas_.document().annotations()[
          canvas_.document().selected()];
      selected.filled = canvas_.style().filled;
    }
  }
  canvas_.setSelection(selection_);
  selectionChanged();
}

bool RegionSelector::editorPress(RegionOverlay *overlay, const QPointF &global,
                                 const QPointF &local, Qt::MouseButton,
                                 Qt::KeyboardModifiers modifiers) {
  Q_UNUSED(overlay)
  if (!hasSelection() || !editing_) {
    beginSelection(global);
    return true;
  }
  const QString selectionHandle = handleAt(selection_, global);
  if (!selectionHandle.isEmpty()) {
    drag_ = Drag::ResizeSelection;
    dragOriginalSelection_ = selection_;
    resizeHandle_ = selectionHandle;
    return true;
  }
  dragStart_ = global;
  canvas_.setSelection(selection_);
  ToolEvent event;
  event.pos = global;
  event.local = local;
  event.modifiers = modifiers;
  if (canvas_.currentToolId() == QStringLiteral("select")) {
    if (canvas_.press(event))
      return true;
    if (selection_.contains(global)) {
      dragOriginalSelection_ = selection_;
      drag_ = Drag::MoveSelection;
      return true;
    }
    beginSelection(global);
    return true;
  }
  if (!selection_.contains(global)) {
    setStatus(ui("Draw inside the selection", "请在选区内绘制"));
    return true;
  }
  if (canvas_.press(event))
    return true;
  return true;
}

void RegionSelector::editorMove(RegionOverlay *, const QPointF &global,
                                Qt::KeyboardModifiers modifiers) {
  if (drag_ == Drag::NewSelection) {
    selection_ = normalizedRect(dragStart_, clampPoint(global));
    selectionChanged();
    return;
  }
  if (drag_ == Drag::MoveSelection) {
    selection_ = clampSelection(
        dragOriginalSelection_.translated(global - dragStart_));
    updateOwnerOutput();
    selectionChanged();
    return;
  }
  if (drag_ == Drag::ResizeSelection) {
    resizeSelectionTo(resizeHandle_, clampPoint(global));
    updateOwnerOutput();
    return;
  }
  ToolEvent event;
  event.pos = global;
  event.modifiers = modifiers;
  canvas_.move(event);
}

void RegionSelector::editorRelease(RegionOverlay *, const QPointF &global,
                                   Qt::KeyboardModifiers modifiers) {
  if (drag_ == Drag::NewSelection) {
    if (selection_.width() < kMinSelection &&
        selection_.height() < kMinSelection) {
      selection_ = {};
      editing_ = false;
      drag_ = Drag::None;
      selectionChanged();
      return;
    }
    drag_ = Drag::None;
    finishSelection();
    return;
  }
  if (drag_ == Drag::MoveSelection || drag_ == Drag::ResizeSelection) {
    drag_ = Drag::None;
    resizeHandle_.clear();
    selectionChanged();
    return;
  }
  ToolEvent event;
  event.pos = global;
  event.modifiers = modifiers;
  canvas_.release(event);
}

bool RegionSelector::editorKey(QKeyEvent *event, RegionOverlay *) {
  if (event->key() == Qt::Key_Escape) {
    if (textEditor_) {
      textEditor_->hide();
      textEditor_->deleteLater();
      textEditor_ = nullptr;
    } else if (ocrText_) {
      closeOcrPanel();
    } else if (toolBar_.paletteOpen()) {
      toolBar_.setPaletteOpen(false);
      selectionChanged();
    } else if (canvas_.document().selected() >= 0) {
      canvas_.document().setSelected(-1);
      selectionChanged();
    } else if (canvas_.currentToolId() != QStringLiteral("select")) {
      canvas_.setTool(QStringLiteral("select"));
      selectionChanged();
    } else {
      cancel();
    }
    event->accept();
    return true;
  }
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    if (hasSelection())
      exportSelection(QuickOutputMode::Both);
    event->accept();
    return true;
  }
  if (event->key() == Qt::Key_F &&
      event->modifiers().testFlag(Qt::ControlModifier) &&
      event->modifiers().testFlag(Qt::ShiftModifier)) {
    chooseFont();
    event->accept();
    return true;
  }
  if (event->matches(QKeySequence::Copy)) {
    exportSelection(QuickOutputMode::Copy);
    event->accept();
    return true;
  }
  if (event->matches(QKeySequence::Save)) {
    exportSelection(QuickOutputMode::Save);
    event->accept();
    return true;
  }
  if (event->key() == Qt::Key_Q) {
    toolBar_.setPaletteOpen(!toolBar_.paletteOpen());
    selectionChanged();
    event->accept();
    return true;
  }
  if (canvas_.keyPress(event)) {
    selectionChanged();
    event->accept();
    return true;
  }
  return false;
}

bool RegionSelector::editorWheel(QWheelEvent *event) {
  if (canvas_.wheel(event)) {
    selectionChanged();
    return true;
  }
  return false;
}

void RegionSelector::beginText(const QPointF &global, RegionOverlay *overlay) {
  commitText();
  if (textEditor_)
    textEditor_->deleteLater();
  if (!overlay)
    return;
  textEditor_ = new QLineEdit(overlay);
  textOverlay_ = overlay;
  textPoint_ = global;
  const QPoint origin = overlay->frame()->monitor.geometry.topLeft();
  const QPointF local = global - QPointF(origin);
  textEditor_->setTextMargins(4, 2, 4, 2);
  textEditor_->setStyleSheet(
      QStringLiteral("QLineEdit { color: %1; background: rgba(16,18,24,220); "
                     "border: 1px solid rgba(255,255,255,45); "
                     "border-radius: 5px; padding: 2px 4px; }")
          .arg(canvas_.style().color.name()));
  textEditor_->setFont(
      annotationTextFont(5.0, canvas_.style().fontFamily));
  textEditor_->resize(240, 34);
  textEditor_->move(
      std::clamp(qRound(local.x()), 8,
                 std::max(8, overlay->width() - 248)),
      std::clamp(qRound(local.y() - 16.0), 8,
                 std::max(8, overlay->height() - 42)));
  textEditor_->show();
  textEditor_->setFocus(Qt::ActiveWindowFocusReason);
  connect(textEditor_, &QLineEdit::editingFinished, this,
          &RegionSelector::commitText);
  setStatus(ui("Text · Enter to commit · Esc to cancel",
               "文字 · Enter 提交 · Esc 取消"));
}

void RegionSelector::commitText() {
  if (!textEditor_ || committingText_)
    return;
  committingText_ = true;
  const QString text = textEditor_->text().trimmed();
  if (!text.isEmpty()) {
    canvas_.document().pushUndo();
    Annotation annotation;
    annotation.kind = Annotation::Kind::Text;
    annotation.start = textPoint_;
    annotation.start.ry() +=
        QFontMetricsF(annotationTextFont(5.0, canvas_.style().fontFamily))
        .ascent();
    annotation.text = text;
    annotation.fontFamily = canvas_.style().fontFamily;
    annotation.color = canvas_.style().color;
    annotation.size = 5.0;
    canvas_.document().add(std::move(annotation));
    setStatus(ui("Text added", "文字已添加"));
  }
  textEditor_->hide();
  textEditor_->deleteLater();
  textEditor_ = nullptr;
  textOverlay_ = nullptr;
  committingText_ = false;
  selectionChanged();
}

void RegionSelector::runOcr() {
  if (!hasSelection() || ocrWatcher_.isRunning())
    return;
  const QString outputName = ownerOutput_;
  QString composeError;
  const QImage composed =
      composeSelection(frames_, selection_, selectedScale(), composeError);
  if (composed.isNull()) {
    setStatus(composeError.isEmpty()
                  ? ui("Could not prepare OCR image", "无法准备 OCR 图像")
                  : composeError);
    return;
  }
  ocrOverlay_ = ownerOverlay();
  QVector<Annotation> local = canvas_.document().annotations();
  for (Annotation &annotation : local) {
    annotation.start -= selection_.topLeft();
    annotation.end -= selection_.topLeft();
    for (QPointF &point : annotation.points)
      point -= selection_.topLeft();
  }
  CaptureData capture = editorCaptureFromImage(
      composed, selection_, selectedScale(), outputName);
  setStatus(ui("Reading text…", "正在识别文字…"));
  ocrWatcher_.setFuture(QtConcurrent::run([capture, local] {
    OcrJob result;
    const QImage image =
        renderCapture(capture, QRectF(QPointF(), capture.previewSize), local,
                      BackgroundStyle::None);
    result.text = recognizeText(image, result.error);
    return result;
  }));
}

void RegionSelector::showOcrPanel(const QString &text, RegionOverlay *overlay) {
  closeOcrPanel();
  if (!overlay || text.isEmpty())
    return;
  ocrText_ = new QPlainTextEdit(overlay);
  ocrText_->setPlainText(text);
  ocrText_->setReadOnly(true);
  const QRectF panel =
      QRectF(overlay->width() * 0.2, overlay->height() * 0.2,
             overlay->width() * 0.6, overlay->height() * 0.45);
  ocrText_->setGeometry(panel.toRect());
  ocrText_->show();
  ocrCopy_ = new QPushButton(ui("Copy text", "复制文本"), overlay);
  ocrCopy_->setObjectName(QStringLiteral("primary"));
  ocrClose_ = new QPushButton(ui("Close", "关闭"), overlay);
  ocrCopy_->show();
  ocrClose_->show();
  ocrCopy_->setGeometry(panel.left(), panel.bottom() + 8, 110, 32);
  ocrClose_->setGeometry(panel.left() + 120, panel.bottom() + 8, 90, 32);
  connect(ocrCopy_, &QPushButton::clicked, this, [this] {
    QString error;
    if (!copyTextToClipboard(ocrText_->toPlainText(), error))
      setStatus(error);
    else
      setStatus(ui("OCR text copied", "OCR 文本已复制"));
  });
  connect(ocrClose_, &QPushButton::clicked, this, &RegionSelector::closeOcrPanel);
  setStatus(ui("OCR ready · select text and copy",
               "OCR 完成 · 选择文本并复制"));
}

void RegionSelector::closeOcrPanel() {
  if (ocrText_)
    ocrText_->deleteLater();
  if (ocrCopy_)
    ocrCopy_->deleteLater();
  if (ocrClose_)
    ocrClose_->deleteLater();
  ocrText_ = nullptr;
  ocrCopy_ = nullptr;
  ocrClose_ = nullptr;
}

void RegionSelector::exportSelection(QuickOutputMode mode) {
  if (!hasSelection()) {
    setStatus(ui("Select a region first", "请先选择区域"));
    return;
  }
  QString error;
  const qreal scale = selectedScale();
  const QString outputName = ownerOutput_;
  QImage composed = composeSelection(frames_, selection_, scale, error);
  if (composed.isNull()) {
    setStatus(error);
    closeAll();
    emit finished(false, error);
    return;
  }
  QVector<Annotation> local = canvas_.document().annotations();
  for (Annotation &annotation : local) {
    annotation.start -= selection_.topLeft();
    annotation.end -= selection_.topLeft();
    for (QPointF &point : annotation.points)
      point -= selection_.topLeft();
  }
  CaptureData capture = editorCaptureFromImage(
      composed, selection_, scale, outputName);
  const QImage image =
      renderCapture(capture, QRectF(QPointF(), capture.previewSize), local,
                    BackgroundStyle::None);
  if (image.isNull() || !quickOutput(image, mode, error)) {
    if (error.isEmpty())
      error = ui("Could not write screenshot", "无法写入截图");
    setStatus(error);
    closeAll();
    emit finished(false, error);
    return;
  }
  closeAll();
  emit finished(true, {});
}

void RegionSelector::editorRightClick() {
  if (textEditor_) {
    textEditor_->hide();
    textEditor_->deleteLater();
    textEditor_ = nullptr;
  } else if (toolBar_.paletteOpen()) {
    toolBar_.setPaletteOpen(false);
  } else if (canvas_.document().selected() >= 0) {
    canvas_.document().setSelected(-1);
  } else if (canvas_.currentToolId() != QStringLiteral("select")) {
    canvas_.setTool(QStringLiteral("select"));
  } else {
    cancel();
    return;
  }
  selectionChanged();
}

void RegionSelector::paintSourceEffects(QPainter &painter,
                                        RegionOverlay *overlay,
                                        const QVector<Annotation> &extra) {
  const OutputFrame *frame = overlay->frame();
  const QPoint origin = frame->monitor.geometry.topLeft();
  QVector<Annotation> all = canvas_.document().annotations();
  all += extra;
  for (const Annotation &annotation : all) {
    if (annotation.kind != Annotation::Kind::Redaction &&
        annotation.kind != Annotation::Kind::Blur)
      continue;
    const QRectF globalRect =
        QRectF(annotation.start, annotation.end).normalized();
    const QRectF intersection =
        globalRect.intersected(selection_).intersected(
            QRectF(frame->monitor.geometry));
    if (intersection.isEmpty() || frame->image.isNull())
      continue;
    const QRectF local = intersection.translated(-origin);
    const qreal sx = frame->image.width() /
                     std::max<qreal>(1, frame->monitor.geometry.width());
    const qreal sy = frame->image.height() /
                     std::max<qreal>(1, frame->monitor.geometry.height());
    const QRect source(
        qFloor((intersection.left() - frame->monitor.geometry.left()) * sx),
        qFloor((intersection.top() - frame->monitor.geometry.top()) * sy),
        std::max(1, qCeil(intersection.width() * sx)),
        std::max(1, qCeil(intersection.height() * sy)));
    const QImage patch = frame->image.copy(source);
    painter.save();
    painter.setClipRect(local);
    if (annotation.kind == Annotation::Kind::Blur) {
      const QSize small(std::max(1, qRound(local.width() / 14.0)),
                        std::max(1, qRound(local.height() / 14.0)));
      const QImage preview =
          patch.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
              .scaled(local.size().toSize(), Qt::IgnoreAspectRatio,
                      Qt::SmoothTransformation);
      painter.drawImage(local.topLeft(), preview);
    } else {
      const QImage preview =
          patch.scaled(QSize(std::max(1, qRound(local.width() / 12.0)),
                             std::max(1, qRound(local.height() / 12.0))),
                       Qt::IgnoreAspectRatio, Qt::FastTransformation)
              .scaled(local.size().toSize(), Qt::IgnoreAspectRatio,
                      Qt::FastTransformation);
      painter.drawImage(local.topLeft(), preview);
    }
    painter.restore();
  }
}

void RegionSelector::paintSelectionChrome(QPainter &painter,
                                          RegionOverlay *overlay) {
  const QPoint origin = overlay->frame()->monitor.geometry.topLeft();
  const QRectF local = selection_.translated(-origin);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(QColor(0, 0, 0, 150), 3));
  painter.drawRect(local.adjusted(-0.5, -0.5, 0.5, 0.5));
  painter.setPen(QPen(QColor(255, 255, 255, 240), 1.5));
  painter.drawRect(local);

  const qreal x0 = local.left();
  const qreal x1 = local.right();
  const qreal y0 = local.top();
  const qreal y1 = local.bottom();
  const qreal cx = local.center().x();
  const qreal cy = local.center().y();
  const std::array<QPointF, 8> handles{
      QPointF(x0, y0), QPointF(cx, y0), QPointF(x1, y0), QPointF(x1, cy),
      QPointF(x1, y1), QPointF(cx, y1), QPointF(x0, y1), QPointF(x0, cy)};
  for (const QPointF &handle : handles) {
    if (!overlay->rect().adjusted(-8, -8, 8, 8).contains(handle.toPoint()))
      continue;
    painter.setPen(QPen(QColor(0, 0, 0, 130), 1));
    painter.setBrush(QColor(250, 252, 255));
    painter.drawRoundedRect(
        QRectF(handle.x() - kHandleSize / 2.0,
               handle.y() - kHandleSize / 2.0, kHandleSize, kHandleSize),
        2.5, 2.5);
  }

  QFont sizeFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  sizeFont.setPixelSize(12);
  sizeFont.setBold(true);
  painter.setFont(sizeFont);
  const QString sizeText =
      QStringLiteral("%1 × %2").arg(qRound(selection_.width()))
          .arg(qRound(selection_.height()));
  const qreal width = QFontMetricsF(sizeFont).horizontalAdvance(sizeText) + 24;
  QRectF label(std::clamp(local.left(), 8.0, overlay->width() - width - 8.0),
               std::max<qreal>(8, local.top() - 36), width, 28);
  drawPill(painter, label, sizeText);
}

void RegionSelector::paintEditor(QPainter &painter, RegionOverlay *overlay) {
  if (!hasSelection())
    return;
  canvas_.setSelection(selection_);
  QVector<Annotation> extra;
  if (const auto preview = canvas_.preview())
    extra.push_back(*preview);
  paintSourceEffects(painter, overlay, extra);
  canvas_.paintVectors(painter, overlay->frame()->monitor.geometry.topLeft());
  paintSelectionChrome(painter, overlay);
  if (overlay->outputName() == ownerOutput_) {
    syncToolbar();
    const QPoint origin = overlay->frame()->monitor.geometry.topLeft();
    toolBar_.paint(painter, QRectF(0, 0, overlay->width(), overlay->height()),
                   selection_.translated(-origin));
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPixelSize(12);
    painter.setFont(font);
    const QString text = status_.isEmpty()
                             ? ui("Drag handles to resize", "拖动控制点调整大小")
                             : status_;
    const qreal width = QFontMetricsF(font).horizontalAdvance(text) + 28;
    drawPill(painter, QRectF((overlay->width() - width) / 2.0,
                             overlay->height() - 46, width, 30),
             text);
  }
}
