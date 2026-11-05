/** @fileoverview Multi-output frozen-frame region selection. */
#include "region-selector.hpp"

#include <LayerShellQt/Window>

#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr qreal kMinSelection = 4.0;
constexpr qreal kHandleSize = 10.0;
constexpr qreal kHandleHit = 16.0;
constexpr int kDimAlpha = 120;
const QColor kPanel(16, 18, 24, 238);
const QColor kPanelBorder(255, 255, 255, 34);

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

void drawPill(QPainter &painter, const QRectF &rect, const QString &text,
              const QColor &textColor = QColor(245, 247, 250)) {
  painter.setPen(QPen(kPanelBorder, 1));
  painter.setBrush(kPanel);
  painter.drawRoundedRect(rect, 9, 9);
  painter.setPen(textColor);
  painter.drawText(rect, Qt::AlignCenter, text);
}
} // namespace

/** A full-output layer surface showing one frozen frame. */
class RegionOverlay final : public QWidget {
public:
  RegionOverlay(RegionSelector *selector, const OutputFrame *frame,
                QWidget *parent = nullptr)
      : QWidget(parent), selector_(selector), frame_(frame) {
    setWindowTitle(QStringLiteral("NiriCapture region selection"));
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
    QRectF local = globalSelection.translated(-origin);
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

    if (!selector_->hasSelection())
      return;

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 150), 3));
    painter.drawRect(local.adjusted(-0.5, -0.5, 0.5, 0.5));
    painter.setPen(QPen(QColor(255, 255, 255, 240), 1.5));
    painter.drawRect(local);

    // The handles live at the selection corners and edge midpoints. They are
    // drawn on every output so a cross-output selection stays adjustable from
    // either side.
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
      if (!rect().adjusted(-8, -8, 8, 8).contains(handle.toPoint()))
        continue;
      painter.setPen(QPen(QColor(0, 0, 0, 130), 1));
      painter.setBrush(QColor(250, 252, 255));
      painter.drawRoundedRect(
          QRectF(handle.x() - kHandleSize / 2.0,
                 handle.y() - kHandleSize / 2.0, kHandleSize, kHandleSize),
          2.5, 2.5);
    }

    if (selector_->ownerOutput_ == frame_->monitor.name) {
      QFont hintFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
      hintFont.setPixelSize(12);
      painter.setFont(hintFont);
      const QString hint =
          selector_->hasSelection()
              ? QStringLiteral("Enter to edit  ·  Esc to cancel  ·  drag handles to resize")
              : QStringLiteral("Drag to select a region  ·  Esc to cancel");
      const qreal width = QFontMetricsF(hintFont).horizontalAdvance(hint) + 28;
      drawPill(painter, QRectF((this->width() - width) / 2.0,
                               this->height() - 46, width, 30),
               hint);
    }

    if (!visible.isEmpty()) {
      const qreal pixelScale = selector_->selectedScale();
      const QString sizeText =
          QStringLiteral("%1 × %2  ·  %3 × %4 px")
              .arg(qRound(globalSelection.width()))
              .arg(qRound(globalSelection.height()))
              .arg(qRound(globalSelection.width() * pixelScale))
              .arg(qRound(globalSelection.height() * pixelScale));
      QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
      font.setPixelSize(12);
      font.setBold(true);
      painter.setFont(font);
      const QFontMetricsF metrics(font);
      const qreal width = metrics.horizontalAdvance(sizeText) + 24;
      const qreal height = 28;
      QRectF label(std::clamp(visible.left(), 8.0, this->width() - width - 8.0),
                   visible.top() - height - 8,
                   width, height);
      if (label.top() < 8)
        label.moveTop(std::min<qreal>(this->height() - height - 8,
                                      visible.bottom() + 8));
      drawPill(painter, label, sizeText);
    }
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Escape) {
      selector_->cancel();
      event->accept();
      return;
    }
    if ((event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter) &&
        selector_->hasSelection()) {
      selector_->accept();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_F) {
      selector_->selection_ = selector_->frames_.logicalBounds;
      selector_->selectionChanged();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
        event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
      if (!selector_->hasSelection()) {
        event->ignore();
        return;
      }
      const qreal step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
      QPointF delta;
      if (event->key() == Qt::Key_Left)
        delta.setX(-step);
      else if (event->key() == Qt::Key_Right)
        delta.setX(step);
      else if (event->key() == Qt::Key_Up)
        delta.setY(-step);
      else
        delta.setY(step);
      selector_->moveBy(delta);
      event->accept();
      return;
    }
    QWidget::keyPressEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::RightButton) {
      selector_->cancel();
      event->accept();
      return;
    }
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    selector_->setKeyboardOverlay(this);
    const QPointF global = toGlobal(event->position());
    selector_->dragStart_ = global;
    selector_->dragOriginal_ = selector_->selection_;
    const QString handle = handleAt(selector_->selection_, global);
    if (!handle.isEmpty()) {
      selector_->dragMode_ = RegionSelector::DragMode::Resize;
      selector_->resizeHandle_ = handle;
    } else if (selector_->hasSelection() &&
               selector_->selection_.contains(global)) {
      selector_->dragMode_ = RegionSelector::DragMode::Move;
    } else {
      selector_->dragMode_ = RegionSelector::DragMode::New;
      selector_->selection_ = QRectF(global, QSizeF(0, 0));
      selector_->selectionChanged();
    }
    event->accept();
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    const QPointF global = toGlobal(event->position());
    switch (selector_->dragMode_) {
    case RegionSelector::DragMode::None:
      updateCursor(global);
      break;
    case RegionSelector::DragMode::New:
      selector_->selection_ =
          normalizedRect(selector_->dragStart_, selector_->clampPoint(global));
      selector_->selectionChanged();
      break;
    case RegionSelector::DragMode::Move:
      selector_->selection_ =
          selector_->clampSelection(selector_->dragOriginal_.translated(
              global - selector_->dragStart_));
      selector_->selectionChanged();
      break;
    case RegionSelector::DragMode::Resize:
      selector_->resizeTo(selector_->resizeHandle_,
                          selector_->clampPoint(global));
      break;
    }
    event->accept();
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() != Qt::LeftButton) {
      QWidget::mouseReleaseEvent(event);
      return;
    }
    if (selector_->dragMode_ == RegionSelector::DragMode::New &&
        selector_->selection_.width() < kMinSelection &&
        selector_->selection_.height() < kMinSelection) {
      selector_->selection_ = {};
      selector_->selectionChanged();
    }
    selector_->dragMode_ = RegionSelector::DragMode::None;
    selector_->resizeHandle_.clear();
    updateCursor(toGlobal(event->position()));
    event->accept();
  }

private:
  QPointF toGlobal(const QPointF &local) const {
    return local + QPointF(frame_->monitor.geometry.topLeft());
  }

  void updateCursor(const QPointF &global) {
    const QString handle = handleAt(selector_->selection_, global);
    if (handle == QStringLiteral("tl") || handle == QStringLiteral("br"))
      setCursor(Qt::SizeFDiagCursor);
    else if (handle == QStringLiteral("tr") ||
             handle == QStringLiteral("bl"))
      setCursor(Qt::SizeBDiagCursor);
    else if (handle == QStringLiteral("l") || handle == QStringLiteral("r"))
      setCursor(Qt::SizeHorCursor);
    else if (handle == QStringLiteral("t") || handle == QStringLiteral("b"))
      setCursor(Qt::SizeVerCursor);
    else if (selector_->hasSelection() &&
             selector_->selection_.contains(global))
      setCursor(Qt::OpenHandCursor);
    else
      setCursor(Qt::CrossCursor);
  }

  RegionSelector *selector_;
  const OutputFrame *frame_;
};

RegionSelector::RegionSelector(DesktopFrames frames, QObject *parent)
    : QObject(parent), frames_(std::move(frames)) {}

RegionSelector::~RegionSelector() {
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

QPointF RegionSelector::clampPoint(const QPointF &point) const {
  const QRectF bounds = frames_.logicalBounds;
  return {std::clamp(point.x(), bounds.left(), bounds.right()),
          std::clamp(point.y(), bounds.top(), bounds.bottom())};
}

QRectF RegionSelector::clampSelection(const QRectF &rect) const {
  QRectF result = rect;
  const QRectF bounds = frames_.logicalBounds;
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
  for (RegionOverlay *overlay : overlays_)
    overlay->update();
}

void RegionSelector::moveBy(const QPointF &delta) {
  if (!hasSelection())
    return;
  selection_ = clampSelection(selection_.translated(delta));
  selectionChanged();
}

void RegionSelector::resizeTo(const QString &handle, const QPointF &global) {
  QRectF rect = dragOriginal_;
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

void RegionSelector::setKeyboardOverlay(RegionOverlay *overlay) {
  ownerOutput_ = overlay->outputName();
  for (RegionOverlay *candidate : overlays_)
    candidate->setKeyboardOwner(candidate == overlay);
}

void RegionSelector::accept() {
  if (finished_ || !hasSelection())
    return;
  finished_ = true;
  const QRectF chosen = selection_;
  const qreal scale = selectedScale();
  for (RegionOverlay *overlay : overlays_)
    overlay->hide();
  emit accepted(chosen, scale);
}

void RegionSelector::cancel() {
  if (finished_)
    return;
  finished_ = true;
  for (RegionOverlay *overlay : overlays_)
    overlay->hide();
  emit cancelled();
}

void RegionSelector::start() {
  if (started_ || frames_.outputs.isEmpty())
    return;
  started_ = true;
  QString focused;
  QString focusError;
  if (!probeFocusedOutputName(focused, focusError))
    focused = frames_.outputs.first().monitor.name;

  ownerOutput_ = focused;
  for (const OutputFrame &frame : frames_.outputs) {
    auto *overlay = new RegionOverlay(this, &frame);
    overlay->configureLayer();
    overlays_.push_back(overlay);
  }
  for (RegionOverlay *overlay : overlays_)
    overlay->show();
  for (RegionOverlay *overlay : overlays_)
    overlay->setKeyboardOwner(overlay->outputName() == focused);
  selectionChanged();

  // Test hook: lets the integration suite exercise the real layer surfaces
  // without a pointer-injection daemon.
  const QString testSelection =
      qEnvironmentVariable("NIRICAPTURE_TEST_SELECT").trimmed();
  if (!testSelection.isEmpty()) {
    const QStringList parts = testSelection.split(QLatin1Char(','));
    if (parts.size() == 4) {
      selection_ = QRectF(parts.at(0).toDouble(), parts.at(1).toDouble(),
                          parts.at(2).toDouble(), parts.at(3).toDouble());
      const int delay = qEnvironmentVariableIntValue("NIRICAPTURE_TEST_SELECT_DELAY");
      QTimer::singleShot(delay > 0 ? delay : 250, this, [this] {
        selectionChanged();
        accept();
      });
    }
  }
  if (qEnvironmentVariableIsSet("NIRICAPTURE_TEST_CANCEL"))
    QTimer::singleShot(200, this, &RegionSelector::cancel);
}
