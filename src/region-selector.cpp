/** @fileoverview Frozen-frame selection overlay with an in-place toolbar. */
#include "region-selector.hpp"

#include "i18n.hpp"
#include "icons.hpp"

#include <LayerShellQt/Window>

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHash>
#include <QKeySequence>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLineF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr qreal kMinSelection = 4.0;
constexpr qreal kHandleSize = 10.0;
constexpr qreal kHandleHit = 16.0;
constexpr int kDimAlpha = 120;
constexpr qreal kButton = 34.0;
constexpr qreal kButtonGap = 4.0;
constexpr qreal kToolbarHeight = 38.0;
constexpr int kToolbarButtons = 18;

const QColor kPanel(17, 19, 24, 246);
const QColor kPanelBorder(255, 255, 255, 30);
const QColor kAccent(124, 196, 255);

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

qreal distanceToSegment(const QPointF &point, const QPointF &a,
                        const QPointF &b) {
  const QPointF ab = b - a;
  const qreal lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
  if (lengthSquared <= 0.0001)
    return QLineF(point, a).length();
  const QPointF ap = point - a;
  const qreal t = std::clamp(
      (ap.x() * ab.x() + ap.y() * ab.y()) / lengthSquared, 0.0, 1.0);
  return QLineF(point, a + ab * t).length();
}

bool isDrawTool(RegionSelector::Tool tool) {
  switch (tool) {
  case RegionSelector::Tool::Pen:
  case RegionSelector::Tool::Arrow:
  case RegionSelector::Tool::Line:
  case RegionSelector::Tool::Rectangle:
  case RegionSelector::Tool::Ellipse:
  case RegionSelector::Tool::Highlighter:
  case RegionSelector::Tool::Mosaic:
  case RegionSelector::Tool::Blur:
    return true;
  default:
    return false;
  }
}

QString toolName(RegionSelector::Tool tool) {
  switch (tool) {
  case RegionSelector::Tool::Select:
    return ui("Select", "选择");
  case RegionSelector::Tool::Pen:
    return ui("Pen", "画笔");
  case RegionSelector::Tool::Arrow:
    return ui("Arrow", "箭头");
  case RegionSelector::Tool::Line:
    return ui("Line", "直线");
  case RegionSelector::Tool::Rectangle:
    return ui("Rectangle", "矩形");
  case RegionSelector::Tool::Ellipse:
    return ui("Ellipse", "椭圆");
  case RegionSelector::Tool::Text:
    return ui("Text", "文字");
  case RegionSelector::Tool::Highlighter:
    return ui("Highlighter", "高亮");
  case RegionSelector::Tool::Mosaic:
    return ui("Mosaic", "马赛克");
  case RegionSelector::Tool::Blur:
    return ui("Blur", "模糊");
  case RegionSelector::Tool::Marker:
    return ui("Marker", "序号");
  }
  return {};
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

  void setHoverAction(const QString &action) {
    if (hoverAction_ == action)
      return;
    hoverAction_ = action;
    update();
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
    QString action;
    if (selector_->toolbarHit(this, local, &action)) {
      selector_->handleAction(action);
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
    selector_->editorMove(this, global);
    if (!selector_->hasSelection())
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
    selector_->editorRelease(this, global);
    event->accept();
  }

  void wheelEvent(QWheelEvent *event) override {
    if (!selector_->hasSelection()) {
      event->ignore();
      return;
    }
    const qreal delta = event->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(delta)) {
      event->ignore();
      return;
    }
    if (selector_->selected_ >= 0 &&
        selector_->selected_ < selector_->annotations_.size()) {
      selector_->pushUndo();
      Annotation &annotation =
          selector_->annotations_[selector_->selected_];
      const qreal factor = delta > 0 ? 1.12 : 1.0 / 1.12;
      annotation.size = std::clamp(annotation.size * factor, 2.0, 96.0);
      selector_->setStatus(ui("Scale %1", "缩放 %1")
                               .arg(qRound(annotation.size)));
      selector_->selectionChanged();
    } else if (selector_->tool_ == RegionSelector::Tool::Blur ||
               selector_->tool_ == RegionSelector::Tool::Mosaic) {
      selector_->width_ = std::clamp(selector_->width_ + delta * 3.0, 2.0, 64.0);
      selector_->setStatus(ui("Effect size %1", "效果大小 %1")
                               .arg(qRound(selector_->width_)));
      selector_->selectionChanged();
    }
    event->accept();
  }

private:
  RegionSelector *selector_;
  const OutputFrame *frame_;
  QString hoverAction_;
};

RegionSelector::RegionSelector(DesktopFrames frames, QuickOutputMode quickMode,
                               QObject *parent)
    : QObject(parent), frames_(std::move(frames)), quickMode_(quickMode) {
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
      const int delay =
          qEnvironmentVariableIntValue("NIRICAPTURE_TEST_SELECT_DELAY");
      QTimer::singleShot(delay > 0 ? delay : 250, this, [this] {
        selectionChanged();
        if (quickMode_ != QuickOutputMode::None)
          exportSelection(quickMode_);
      });
    }
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
  result.moveLeft(std::clamp(result.left(), bounds.left(),
                             bounds.right() - result.width()));
  result.moveTop(std::clamp(result.top(), bounds.top(),
                            bounds.bottom() - result.height()));
  return result;
}

void RegionSelector::selectionChanged() {
  for (RegionOverlay *overlay : overlays_)
    overlay->update();
}

void RegionSelector::setStatus(const QString &status) {
  status_ = status;
  selectionChanged();
}

void RegionSelector::beginSelection(const QPointF &global) {
  annotations_.clear();
  undo_.clear();
  redo_.clear();
  selected_ = -1;
  nextMarker_ = 1;
  selection_ = QRectF(clampToBounds(global, frames_.logicalBounds),
                      QSizeF(0, 0));
  drag_ = Drag::NewSelection;
  dragStart_ = global;
  dragCurrent_ = global;
  editing_ = false;
  selectionChanged();
}

void RegionSelector::moveSelectionBy(const QPointF &delta) {
  if (!hasSelection())
    return;
  selection_ = clampSelectionToBounds(selection_.translated(delta),
                                      frames_.logicalBounds);
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
  selection_ = clampSelectionToBounds(
      QRectF(QPointF(left, top), QPointF(right, bottom)).normalized(),
      frames_.logicalBounds);
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

void RegionSelector::pushUndo() {
  undo_.push_back(annotations_);
  if (undo_.size() > 100)
    undo_.removeFirst();
  redo_.clear();
}

void RegionSelector::undo() {
  if (undo_.isEmpty() || finished_)
    return;
  redo_.push_back(annotations_);
  annotations_ = undo_.takeLast();
  selected_ = -1;
  setStatus(ui("Undo", "已撤销"));
}

void RegionSelector::redo() {
  if (redo_.isEmpty() || finished_)
    return;
  undo_.push_back(annotations_);
  annotations_ = redo_.takeLast();
  selected_ = -1;
  setStatus(ui("Redo", "已重做"));
}

void RegionSelector::deleteSelected() {
  if (selected_ < 0 || selected_ >= annotations_.size())
    return;
  pushUndo();
  annotations_.removeAt(selected_);
  selected_ = -1;
  setStatus(ui("Annotation deleted", "已删除标注"));
}

QRectF RegionSelector::annotationBounds(const Annotation &annotation) const {
  if (annotation.kind == Annotation::Kind::Marker) {
    const qreal size = std::max<qreal>(6.0, annotation.size);
    return QRectF(annotation.start.x() - size, annotation.start.y() - size,
                  size * 2, size * 2);
  }
  if (annotation.kind == Annotation::Kind::Text) {
    const QFont font = annotationTextFont(annotation.size);
    const QFontMetricsF metrics(font);
    const qreal width = metrics.horizontalAdvance(annotation.text);
    const qreal height = metrics.height();
    return QRectF(annotation.start.x(), annotation.start.y() - metrics.ascent(),
                  std::max<qreal>(4.0, width), height);
  }
  if (annotation.kind == Annotation::Kind::Freehand ||
      annotation.kind == Annotation::Kind::Highlighter) {
    if (annotation.points.isEmpty())
      return {};
    QRectF bounds(annotation.points.first(), QSizeF(0, 0));
    for (const QPointF &point : annotation.points)
      bounds = bounds.united(QRectF(point, QSizeF(0, 0)));
    const qreal pad = std::max<qreal>(5.0, annotation.size + 4.0);
    return bounds.adjusted(-pad, -pad, pad, pad);
  }
  QRectF result = QRectF(annotation.start, annotation.end).normalized();
  const qreal pad = std::max<qreal>(4.0, annotation.size + 2.0);
  return result.adjusted(-pad, -pad, pad, pad);
}

int RegionSelector::annotationAt(const QPointF &point) const {
  for (int index = annotations_.size() - 1; index >= 0; --index) {
    const Annotation &annotation = annotations_.at(index);
    if (annotation.kind == Annotation::Kind::Text ||
        annotation.kind == Annotation::Kind::Marker ||
        annotation.kind == Annotation::Kind::Redaction ||
        annotation.kind == Annotation::Kind::Blur) {
      if (annotationBounds(annotation).contains(point))
        return index;
      continue;
    }
    if (annotation.kind == Annotation::Kind::Freehand ||
        annotation.kind == Annotation::Kind::Highlighter) {
      for (int i = 1; i < annotation.points.size(); ++i) {
        if (distanceToSegment(point, annotation.points.at(i - 1),
                              annotation.points.at(i)) <=
            std::max<qreal>(8.0, annotation.size + 3.0))
          return index;
      }
      continue;
    }
    if (annotation.kind == Annotation::Kind::Line ||
        annotation.kind == Annotation::Kind::Arrow) {
      if (distanceToSegment(point, annotation.start, annotation.end) <=
          std::max<qreal>(8.0, annotation.size + 3.0))
        return index;
      continue;
    }
    if (annotation.kind == Annotation::Kind::Ellipse) {
      QRectF bounds = QRectF(annotation.start, annotation.end).normalized();
      if (bounds.width() < 1 || bounds.height() < 1)
        continue;
      const qreal nx = (point.x() - bounds.center().x()) / (bounds.width() / 2);
      const qreal ny = (point.y() - bounds.center().y()) / (bounds.height() / 2);
      if (annotation.filled) {
        if (nx * nx + ny * ny <= 1.05)
          return index;
      } else {
        const qreal distance = std::abs(std::hypot(nx, ny) - 1.0);
        if (distance <= std::max<qreal>(0.08, (annotation.size + 4.0) /
                                                  std::max(bounds.width(),
                                                           bounds.height())))
          return index;
      }
      continue;
    }
    QRectF bounds = QRectF(annotation.start, annotation.end).normalized();
    if (annotation.filled) {
      if (bounds.contains(point))
        return index;
    } else {
      const qreal tolerance = std::max<qreal>(7.0, annotation.size + 3.0);
      const QRectF outer =
          bounds.adjusted(-tolerance, -tolerance, tolerance, tolerance);
      const QRectF inner =
          bounds.adjusted(tolerance, tolerance, -tolerance, -tolerance);
      if (outer.contains(point) && (inner.isEmpty() || !inner.contains(point)))
        return index;
    }
  }
  return -1;
}

QString RegionSelector::annotationHandleAt(const QPointF &point) const {
  if (selected_ < 0 || selected_ >= annotations_.size())
    return {};
  const Annotation &annotation = annotations_.at(selected_);
  if (annotation.kind == Annotation::Kind::Text ||
      annotation.kind == Annotation::Kind::Marker ||
      annotation.kind == Annotation::Kind::Freehand ||
      annotation.kind == Annotation::Kind::Highlighter)
    return {};
  if (QLineF(point, annotation.start).length() <= kHandleHit)
    return QStringLiteral("start");
  if (QLineF(point, annotation.end).length() <= kHandleHit)
    return QStringLiteral("end");
  return {};
}

Annotation RegionSelector::previewAnnotation() const {
  Annotation annotation;
  annotation.kind = Annotation::Kind::Arrow;
  annotation.color = color_;
  annotation.size = width_;
  annotation.filled = filled_;
  switch (tool_) {
  case Tool::Pen:
    annotation.kind = Annotation::Kind::Freehand;
    annotation.points = freehandPoints_;
    annotation.start = freehandPoints_.isEmpty() ? dragStart_
                                                 : freehandPoints_.first();
    annotation.end =
        freehandPoints_.isEmpty() ? dragStart_ : freehandPoints_.last();
    break;
  case Tool::Highlighter:
    annotation.kind = Annotation::Kind::Highlighter;
    annotation.color.setAlpha(110);
    annotation.points = freehandPoints_;
    annotation.start = freehandPoints_.isEmpty() ? dragStart_
                                                 : freehandPoints_.first();
    annotation.end =
        freehandPoints_.isEmpty() ? dragStart_ : freehandPoints_.last();
    break;
  case Tool::Arrow:
    annotation.kind = Annotation::Kind::Arrow;
    annotation.start = dragStart_;
    annotation.end = dragCurrent_;
    break;
  case Tool::Line:
    annotation.kind = Annotation::Kind::Line;
    annotation.start = dragStart_;
    annotation.end = dragCurrent_;
    break;
  case Tool::Rectangle:
    annotation.kind = Annotation::Kind::Rectangle;
    annotation.start = dragStart_;
    annotation.end = dragCurrent_;
    break;
  case Tool::Ellipse:
    annotation.kind = Annotation::Kind::Ellipse;
    annotation.start = dragStart_;
    annotation.end = dragCurrent_;
    break;
  case Tool::Mosaic:
    annotation.kind = Annotation::Kind::Redaction;
    annotation.start = dragStart_;
    annotation.end = dragCurrent_;
    annotation.redactionStyle = RedactionStyle::Pixelate;
    annotation.redactionSeed = 1;
    break;
  case Tool::Blur:
    annotation.kind = Annotation::Kind::Blur;
    annotation.start = dragStart_;
    annotation.end = dragCurrent_;
    break;
  default:
    break;
  }
  return annotation;
}

void RegionSelector::paintAnnotations(QPainter &painter, RegionOverlay *overlay,
                                      const QVector<Annotation> &extra) {
  const OutputFrame *frame = overlay->frame();
  const QPoint origin = frame->monitor.geometry.topLeft();
  QVector<Annotation> all = annotations_;
  all += extra;

  // Source effects are painted from the frozen image preview.
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
    const qreal sx =
        frame->image.width() / std::max<qreal>(1, frame->monitor.geometry.width());
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
      const QImage preview = patch.scaled(small, Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation)
                                 .scaled(local.size().toSize(),
                                         Qt::IgnoreAspectRatio,
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

  painter.save();
  painter.translate(-origin.x(), -origin.y());
  painter.setClipRect(selection_);
  for (const Annotation &annotation : all) {
    if (annotation.kind == Annotation::Kind::Redaction ||
        annotation.kind == Annotation::Kind::Blur)
      continue;
    paintAnnotation(painter, annotation);
  }
  painter.restore();

  if (selected_ >= 0 && selected_ < annotations_.size()) {
    const Annotation &annotation = annotations_.at(selected_);
    if (annotation.kind != Annotation::Kind::Text &&
        annotation.kind != Annotation::Kind::Marker &&
        annotation.kind != Annotation::Kind::Freehand &&
        annotation.kind != Annotation::Kind::Highlighter) {
      painter.save();
      painter.translate(-origin.x(), -origin.y());
      painter.setPen(QPen(kAccent, 1.5, Qt::DashLine));
      painter.setBrush(Qt::NoBrush);
      const QRectF bounds =
          QRectF(annotation.start, annotation.end).normalized();
      painter.drawRect(bounds);
      painter.setBrush(kAccent);
      painter.setPen(Qt::NoPen);
      for (const QPointF &point : {annotation.start, annotation.end})
        painter.drawEllipse(point, 4, 4);
      painter.restore();
    }
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

QRectF RegionSelector::toolbarRect(RegionOverlay *overlay) const {
  const qreal width =
      kToolbarButtons * kButton + (kToolbarButtons - 1) * kButtonGap;
  const QPoint origin = overlay->frame()->monitor.geometry.topLeft();
  const QRectF local = selection_.translated(-origin);
  qreal x = local.center().x() - width / 2.0;
  x = std::clamp(x, 8.0, std::max(8.0, overlay->width() - width - 8.0));
  qreal y = local.top() - kToolbarHeight - 10;
  if (y < 8)
    y = local.bottom() + 10;
  if (y + kToolbarHeight > overlay->height() - 8)
    y = std::clamp(local.top() + 8, 8.0,
                   std::max(8.0, overlay->height() - kToolbarHeight - 8));
  return {x, y, width, kToolbarHeight};
}

QRectF RegionSelector::popupRect(RegionOverlay *overlay) const {
  const QRectF toolbar = toolbarRect(overlay);
  const qreal width = 286;
  qreal x = std::clamp(toolbar.center().x() - width / 2.0, 8.0,
                       std::max(8.0, overlay->width() - width - 8.0));
  qreal y = toolbar.bottom() + 6;
  if (y + 76 > overlay->height() - 8)
    y = toolbar.top() - 82;
  return {x, std::max<qreal>(8, y), width, 76};
}

QVector<RegionSelector::Button>
RegionSelector::toolbarButtons(RegionOverlay *overlay) const {
  QVector<Button> buttons;
  const QRectF bar = toolbarRect(overlay);
  struct Item {
    const char *action;
    const char *icon;
    const char *tipEn;
    const char *tipZh;
  };
  const std::array<Item, 18> items{{
      {"select", "tool-select", "Select", "选择"},
      {"pen", "tool-freehand", "Pen", "画笔"},
      {"arrow", "tool-arrow", "Arrow", "箭头"},
      {"line", "tool-line", "Line", "直线"},
      {"rectangle", "tool-rectangle", "Rectangle", "矩形"},
      {"ellipse", "tool-ellipse", "Ellipse", "椭圆"},
      {"text", "tool-text", "Text", "文字"},
      {"highlighter", "tool-highlighter", "Highlighter", "高亮"},
      {"mosaic", "tool-redact", "Mosaic", "马赛克"},
      {"blur", "tool-blur", "Blur", "模糊"},
      {"marker", "tool-marker", "Number", "序号"},
      {"palette", "palette", "Colour / width", "颜色 / 线宽"},
      {"undo", "undo", "Undo", "撤销"},
      {"redo", "redo", "Redo", "重做"},
      {"ocr", "tool-ocr", "OCR", "OCR"},
      {"copy", "copy", "Copy", "复制"},
      {"save", "save", "Save", "保存"},
      {"close", "close", "Close", "关闭"},
  }};
  for (int index = 0; index < static_cast<int>(items.size()); ++index) {
    QRectF rect(bar.left() + index * (kButton + kButtonGap), bar.top(),
                kButton, bar.height());
    buttons.push_back({rect, QString::fromLatin1(items[index].action),
                       ui(items[index].tipEn, items[index].tipZh),
                       items[index].action == QStringLiteral("palette") ? color_
                                                                         : QColor()});
  }
  if (popup_ == Popup::Palette) {
    const QRectF panel = popupRect(overlay);
    for (int index = 0; index < 8; ++index) {
      const std::array<const char *, 8> colors{
          "#ff375f", "#ff9f0a", "#ffd60a", "#30d158",
          "#0a84ff", "#bf5af2", "#ffffff", "#000000"};
      buttons.push_back(
          {QRectF(panel.left() + 8 + index * 32, panel.top() + 7, 24, 24),
           QStringLiteral("color-%1").arg(index), QString(),
           QColor(QString::fromLatin1(colors[index]))});
    }
    const std::array<const char *, 4> sizes{"2", "4", "8", "12"};
    const std::array<const char *, 4> labels{"S", "M", "L", "XL"};
    for (int index = 0; index < 4; ++index) {
      buttons.push_back(
          {QRectF(panel.left() + 8 + index * 58, panel.top() + 40, 52, 26),
           QStringLiteral("size-") + QString::fromLatin1(sizes[index]),
           QString::fromLatin1(labels[index]), QColor()});
    }
    buttons.push_back(
        {QRectF(panel.left() + 8 + 4 * 58, panel.top() + 40, 52, 26),
         QStringLiteral("fill"), ui("Fill", "填充"), QColor()});
    buttons.back().tooltip =
        filled_ ? ui("Fill: on", "填充：开") : ui("Fill: off", "填充：关");
  }
  return buttons;
}

bool RegionSelector::toolbarHit(RegionOverlay *overlay, const QPointF &local,
                                QString *action) {
  if (overlay->outputName() != ownerOutput_ || !hasSelection())
    return false;
  for (const Button &button : toolbarButtons(overlay)) {
    if (button.rect.contains(local)) {
      if (action)
        *action = button.action;
      return true;
    }
  }
  return false;
}

void RegionSelector::handleAction(const QString &action) {
  if (action == QStringLiteral("select")) {
    tool_ = Tool::Select;
    setStatus(ui("Select / move", "选择 / 移动"));
  } else if (action == QStringLiteral("pen")) {
    tool_ = Tool::Pen;
    setStatus(ui("Pen", "画笔"));
  } else if (action == QStringLiteral("arrow")) {
    tool_ = Tool::Arrow;
    setStatus(ui("Arrow", "箭头"));
  } else if (action == QStringLiteral("line")) {
    tool_ = Tool::Line;
    setStatus(ui("Line", "直线"));
  } else if (action == QStringLiteral("rectangle")) {
    tool_ = Tool::Rectangle;
    setStatus(ui("Rectangle", "矩形"));
  } else if (action == QStringLiteral("ellipse")) {
    tool_ = Tool::Ellipse;
    setStatus(ui("Ellipse", "椭圆"));
  } else if (action == QStringLiteral("text")) {
    tool_ = Tool::Text;
    setStatus(ui("Text · click inside the selection", "文字 · 在选区内点击"));
  } else if (action == QStringLiteral("highlighter")) {
    tool_ = Tool::Highlighter;
    setStatus(ui("Highlighter", "高亮"));
  } else if (action == QStringLiteral("mosaic")) {
    tool_ = Tool::Mosaic;
    setStatus(ui("Mosaic", "马赛克"));
  } else if (action == QStringLiteral("blur")) {
    tool_ = Tool::Blur;
    if (width_ < 8)
      width_ = 16;
    setStatus(ui("Gaussian blur", "高斯模糊"));
  } else if (action == QStringLiteral("marker")) {
    tool_ = Tool::Marker;
    setStatus(ui("Number marker · click inside", "序号 · 在选区内点击"));
  } else if (action == QStringLiteral("palette")) {
    popup_ = popup_ == Popup::Palette ? Popup::None : Popup::Palette;
  } else if (action == QStringLiteral("undo")) {
    undo();
  } else if (action == QStringLiteral("redo")) {
    redo();
  } else if (action == QStringLiteral("ocr")) {
    runOcr();
  } else if (action == QStringLiteral("copy")) {
    exportSelection(QuickOutputMode::Copy);
  } else if (action == QStringLiteral("save")) {
    exportSelection(QuickOutputMode::Save);
  } else if (action == QStringLiteral("close")) {
    cancel();
  } else if (action.startsWith(QStringLiteral("color-"))) {
    static const std::array<const char *, 8> colors{
        "#ff375f", "#ff9f0a", "#ffd60a", "#30d158",
        "#0a84ff", "#bf5af2", "#ffffff", "#000000"};
    const int index = action.sliced(6).toInt();
    if (index >= 0 && index < 8) {
      color_ = QColor(QString::fromLatin1(colors[index]));
      if (selected_ >= 0 && selected_ < annotations_.size()) {
        pushUndo();
        annotations_[selected_].color = color_;
      }
    }
  } else if (action.startsWith(QStringLiteral("size-"))) {
    width_ = std::clamp(action.sliced(5).toInt(), 2, 64);
    if (selected_ >= 0 && selected_ < annotations_.size()) {
      pushUndo();
      annotations_[selected_].size = width_;
    }
    setStatus(ui("Size %1", "大小 %1").arg(qRound(width_)));
  } else if (action == QStringLiteral("fill")) {
    filled_ = !filled_;
    if (selected_ >= 0 && selected_ < annotations_.size()) {
      pushUndo();
      annotations_[selected_].filled = filled_;
    }
    setStatus(filled_ ? ui("Fill: on", "填充：开")
                      : ui("Fill: off", "填充：关"));
  }
  selectionChanged();
}

bool RegionSelector::editorPress(RegionOverlay *overlay, const QPointF &global,
                                 const QPointF &local, Qt::MouseButton,
                                 Qt::KeyboardModifiers) {
  Q_UNUSED(local)
  if (!hasSelection() || !editing_) {
    beginSelection(global);
    return true;
  }
  if (popup_ != Popup::None) {
    // Clicking outside the popup closes it but never loses the selection.
    popup_ = Popup::None;
    selectionChanged();
    return true;
  }
  const QString selectionHandle = handleAt(selection_, global);
  if (!selectionHandle.isEmpty()) {
    drag_ = Drag::ResizeSelection;
    dragOriginalSelection_ = selection_;
    resizeHandle_ = selectionHandle;
    return true;
  }
  if (tool_ == Tool::Select) {
    const QString annotationHandle = annotationHandleAt(global);
    if (!annotationHandle.isEmpty() && selected_ >= 0) {
      drag_ = Drag::ResizeAnnotation;
      dragOriginalAnnotation_ = annotations_.at(selected_);
      dragAnnotationIndex_ = selected_;
      dragStart_ = global;
      return true;
    }
    const int hit = annotationAt(global);
    if (hit >= 0) {
      selected_ = hit;
      drag_ = Drag::MoveAnnotation;
      dragOriginalAnnotation_ = annotations_.at(hit);
      dragAnnotationIndex_ = hit;
      dragStart_ = global;
      selectionChanged();
      return true;
    }
    selected_ = -1;
    if (selection_.contains(global)) {
      drag_ = Drag::MoveSelection;
      dragOriginalSelection_ = selection_;
      dragStart_ = global;
    } else {
      beginSelection(global);
    }
    return true;
  }
  if (!selection_.contains(global)) {
    setStatus(ui("Draw inside the selection", "请在选区内绘制"));
    return true;
  }
  if (tool_ == Tool::Text) {
    beginText(global, overlay);
    return true;
  }
  if (tool_ == Tool::Marker) {
    pushUndo();
    Annotation marker;
    marker.kind = Annotation::Kind::Marker;
    marker.start = global;
    marker.color = color_;
    marker.size = std::max<qreal>(12.0, width_ * 2.0);
    marker.number = nextMarker_++;
    annotations_.push_back(std::move(marker));
    selected_ = annotations_.size() - 1;
    setStatus(ui("Number %1", "序号 %1").arg(nextMarker_ - 1));
    return true;
  }
  if (isDrawTool(tool_)) {
    pushUndo();
    drag_ = Drag::Draw;
    dragStart_ = global;
    dragCurrent_ = global;
    freehandPoints_.clear();
    freehandPoints_.push_back(global);
    return true;
  }
  return true;
}

void RegionSelector::editorMove(RegionOverlay *, const QPointF &global) {
  if (drag_ == Drag::None)
    return;
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
  if (drag_ == Drag::MoveAnnotation && dragAnnotationIndex_ >= 0 &&
      dragAnnotationIndex_ < annotations_.size()) {
    Annotation moved = dragOriginalAnnotation_;
    const QPointF delta = global - dragStart_;
    moved.start += delta;
    moved.end += delta;
    for (QPointF &point : moved.points)
      point += delta;
    annotations_[dragAnnotationIndex_] = std::move(moved);
    selectionChanged();
    return;
  }
  if (drag_ == Drag::ResizeAnnotation && dragAnnotationIndex_ >= 0 &&
      dragAnnotationIndex_ < annotations_.size()) {
    Annotation resized = annotations_.at(dragAnnotationIndex_);
    if (QLineF(global, resized.start).length() <
        QLineF(global, resized.end).length())
      resized.start = global;
    else
      resized.end = global;
    annotations_[dragAnnotationIndex_] = std::move(resized);
    selectionChanged();
    return;
  }
  if (drag_ == Drag::Draw) {
    dragCurrent_ = global;
    if (tool_ == Tool::Pen || tool_ == Tool::Highlighter)
      freehandPoints_.push_back(global);
    selectionChanged();
  }
}

void RegionSelector::editorRelease(RegionOverlay *, const QPointF &) {
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
  if (drag_ == Drag::Draw) {
    Annotation annotation = previewAnnotation();
    bool valid = false;
    if (annotation.kind == Annotation::Kind::Freehand ||
        annotation.kind == Annotation::Kind::Highlighter) {
      valid = annotation.points.size() > 1;
    } else {
      valid = std::abs(annotation.end.x() - annotation.start.x()) > 2 ||
              std::abs(annotation.end.y() - annotation.start.y()) > 2;
    }
    if (valid) {
      annotations_.push_back(std::move(annotation));
      selected_ = annotations_.size() - 1;
      setStatus(toolName(tool_));
    } else if (!undo_.isEmpty()) {
      undo_.removeLast();
    }
    drag_ = Drag::None;
    freehandPoints_.clear();
    selectionChanged();
    return;
  }
  drag_ = Drag::None;
  dragAnnotationIndex_ = -1;
  selectionChanged();
}

void RegionSelector::beginText(const QPointF &global, RegionOverlay *overlay) {
  commitText();
  if (textEditor_)
    textEditor_->deleteLater();
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
          .arg(color_.name()));
  textEditor_->setFont(annotationTextFont(5.0));
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
    pushUndo();
    Annotation annotation;
    annotation.kind = Annotation::Kind::Text;
    annotation.start = textPoint_;
    annotation.start.ry() +=
        QFontMetricsF(annotationTextFont(5.0)).ascent();
    annotation.text = text;
    annotation.color = color_;
    annotation.size = 5.0;
    annotations_.push_back(std::move(annotation));
    selected_ = annotations_.size() - 1;
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
  ocrOverlay_ = nullptr;
  for (RegionOverlay *overlay : overlays_) {
    if (overlay->outputName() == outputName) {
      ocrOverlay_ = overlay;
      break;
    }
  }
  QVector<Annotation> local = annotations_;
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
  ocrText_->setStyleSheet(
      QStringLiteral("QPlainTextEdit { color: #f5f7fa; background: rgba(18,20,26,245); "
                     "border: 1px solid rgba(255,255,255,40); "
                     "border-radius: 9px; padding: 9px; }"));
  const QRectF panel =
      QRectF(overlay->width() * 0.2, overlay->height() * 0.2,
             overlay->width() * 0.6, overlay->height() * 0.45);
  ocrText_->setGeometry(panel.toRect());
  ocrText_->show();
  ocrCopy_ = new QPushButton(ui("Copy text", "复制文本"), overlay);
  ocrClose_ = new QPushButton(ui("Close", "关闭"), overlay);
  for (QPushButton *button : {ocrCopy_.data(), ocrClose_.data()}) {
    button->setStyleSheet(
        QStringLiteral("QPushButton { color: #f5f7fa; background: #2a2d35; "
                       "border: 1px solid rgba(255,255,255,35); "
                       "border-radius: 7px; padding: 6px 12px; }"));
    button->show();
  }
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
  QVector<Annotation> local = annotations_;
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

bool RegionSelector::editorKey(QKeyEvent *event, RegionOverlay *) {
  if (event->key() == Qt::Key_Escape) {
    if (textEditor_) {
      textEditor_->hide();
      textEditor_->deleteLater();
      textEditor_ = nullptr;
    } else if (ocrText_) {
      closeOcrPanel();
    } else if (popup_ != Popup::None) {
      popup_ = Popup::None;
      selectionChanged();
    } else if (selected_ >= 0) {
      selected_ = -1;
      selectionChanged();
    } else if (tool_ != Tool::Select) {
      tool_ = Tool::Select;
      setStatus(ui("Select", "选择"));
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
  if (event->matches(QKeySequence::Undo)) {
    undo();
    event->accept();
    return true;
  }
  if (event->matches(QKeySequence::Redo)) {
    redo();
    event->accept();
    return true;
  }
  if (event->key() == Qt::Key_Delete) {
    deleteSelected();
    event->accept();
    return true;
  }
  if (event->key() == Qt::Key_Q) {
    popup_ = popup_ == Popup::Palette ? Popup::None : Popup::Palette;
    selectionChanged();
    event->accept();
    return true;
  }
  const QHash<int, Tool> tools{
      {Qt::Key_V, Tool::Select},       {Qt::Key_F, Tool::Pen},
      {Qt::Key_A, Tool::Arrow},       {Qt::Key_L, Tool::Line},
      {Qt::Key_R, Tool::Rectangle},   {Qt::Key_E, Tool::Ellipse},
      {Qt::Key_T, Tool::Text},        {Qt::Key_H, Tool::Highlighter},
      {Qt::Key_D, Tool::Mosaic},      {Qt::Key_B, Tool::Blur},
      {Qt::Key_C, Tool::Marker},
  };
  if (tools.contains(event->key())) {
    tool_ = tools.value(event->key());
    setStatus(toolName(tool_));
    event->accept();
    return true;
  }
  return false;
}

void RegionSelector::editorRightClick() {
  if (textEditor_) {
    textEditor_->hide();
    textEditor_->deleteLater();
    textEditor_ = nullptr;
  } else if (popup_ != Popup::None) {
    popup_ = Popup::None;
  } else if (selected_ >= 0) {
    selected_ = -1;
  } else if (tool_ != Tool::Select) {
    tool_ = Tool::Select;
  } else {
    cancel();
    return;
  }
  selectionChanged();
}

void RegionSelector::paintEditor(QPainter &painter, RegionOverlay *overlay) {
  if (!hasSelection())
    return;
  paintAnnotations(painter, overlay, drag_ == Drag::Draw
                                          ? QVector<Annotation>{previewAnnotation()}
                                          : QVector<Annotation>{});
  paintSelectionChrome(painter, overlay);
  if (overlay->outputName() == ownerOutput_) {
    paintToolbar(painter, overlay);
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

void RegionSelector::paintToolbar(QPainter &painter, RegionOverlay *overlay) {
  const QRectF bar = toolbarRect(overlay);
  painter.setPen(QPen(kPanelBorder, 1));
  painter.setBrush(kPanel);
  painter.drawRoundedRect(bar.adjusted(-4, -4, 4, 4), 11, 11);

  if (popup_ == Popup::Palette) {
    const QRectF panel = popupRect(overlay);
    painter.setPen(QPen(kPanelBorder, 1));
    painter.setBrush(kPanel);
    painter.drawRoundedRect(panel, 10, 10);
  }

  for (const Button &button : toolbarButtons(overlay)) {
    const bool isTool =
        (button.action == QStringLiteral("select") && tool_ == Tool::Select) ||
        (button.action == QStringLiteral("pen") && tool_ == Tool::Pen) ||
        (button.action == QStringLiteral("arrow") && tool_ == Tool::Arrow) ||
        (button.action == QStringLiteral("line") && tool_ == Tool::Line) ||
        (button.action == QStringLiteral("rectangle") &&
         tool_ == Tool::Rectangle) ||
        (button.action == QStringLiteral("ellipse") && tool_ == Tool::Ellipse) ||
        (button.action == QStringLiteral("text") && tool_ == Tool::Text) ||
        (button.action == QStringLiteral("highlighter") &&
         tool_ == Tool::Highlighter) ||
        (button.action == QStringLiteral("mosaic") && tool_ == Tool::Mosaic) ||
        (button.action == QStringLiteral("blur") && tool_ == Tool::Blur) ||
        (button.action == QStringLiteral("marker") && tool_ == Tool::Marker);
    if (isTool) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(kAccent.red(), kAccent.green(), kAccent.blue(),
                              70));
      painter.drawRoundedRect(button.rect, 7, 7);
    }
    if (button.action.startsWith(QStringLiteral("color-"))) {
      painter.setPen(QPen(QColor(255, 255, 255, 60), 1));
      painter.setBrush(button.color);
      painter.drawEllipse(button.rect.adjusted(3, 3, -3, -3));
      continue;
    }
    if (button.action.startsWith(QStringLiteral("size-")) ||
        button.action == QStringLiteral("fill")) {
      painter.setPen(QColor(240, 243, 248));
      QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
      f.setPixelSize(11);
      painter.setFont(f);
      painter.drawText(button.rect, Qt::AlignCenter, button.tooltip);
      continue;
    }
    QString icon = button.action;
    if (button.action == QStringLiteral("select"))
      icon = QStringLiteral("tool-select");
    else if (button.action == QStringLiteral("pen"))
      icon = QStringLiteral("tool-freehand");
    else if (button.action == QStringLiteral("arrow"))
      icon = QStringLiteral("tool-arrow");
    else if (button.action == QStringLiteral("line"))
      icon = QStringLiteral("tool-line");
    else if (button.action == QStringLiteral("rectangle"))
      icon = QStringLiteral("tool-rectangle");
    else if (button.action == QStringLiteral("ellipse"))
      icon = QStringLiteral("tool-ellipse");
    else if (button.action == QStringLiteral("text"))
      icon = QStringLiteral("tool-text");
    else if (button.action == QStringLiteral("highlighter"))
      icon = QStringLiteral("tool-highlighter");
    else if (button.action == QStringLiteral("mosaic"))
      icon = QStringLiteral("tool-redact");
    else if (button.action == QStringLiteral("blur"))
      icon = QStringLiteral("tool-blur");
    else if (button.action == QStringLiteral("marker"))
      icon = QStringLiteral("tool-marker");
    else if (button.action == QStringLiteral("ocr"))
      icon = QStringLiteral("tool-ocr");
    if (button.action == QStringLiteral("palette")) {
      painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
      painter.setBrush(color_);
      painter.drawEllipse(QRectF(button.rect.center().x() - 8,
                                 button.rect.center().y() - 8, 16, 16));
      continue;
    }
    const bool disabled =
        (button.action == QStringLiteral("undo") && undo_.isEmpty()) ||
        (button.action == QStringLiteral("redo") && redo_.isEmpty());
    drawToolbarIcon(painter, button.rect,
                    disabled ? QString() : icon, {},
                    disabled ? QColor(245, 245, 247, 70)
                             : QColor(245, 245, 247));
  }
}
