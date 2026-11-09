#include "editor/tools/select-tool.hpp"

#include "i18n.hpp"
#include "editor/annotation-document.hpp"
#include "editor/annotation-geometry.hpp"

#include <QKeyEvent>

#include <algorithm>
#include <QLineF>
#include <QWheelEvent>
#include <cmath>

namespace {
QPointF aspectConstrained(const Annotation &original, const QPointF &anchor,
                          const QPointF &point) {
  const qreal width = std::abs(original.end.x() - original.start.x());
  const qreal height = std::abs(original.end.y() - original.start.y());
  if (width < 1.0 || height < 1.0)
    return point;
  const qreal ratio = width / height;
  const qreal dx = point.x() - anchor.x();
  const qreal dy = point.y() - anchor.y();
  qreal x = std::abs(dx);
  qreal y = std::abs(dy);
  if (x / ratio >= y)
    y = x / ratio;
  else
    x = y * ratio;
  return {anchor.x() + (dx < 0 ? -x : x),
          anchor.y() + (dy < 0 ? -y : y)};
}

QPointF angleConstrained(const Annotation &original, const QPointF &anchor,
                         const QPointF &point, bool movingStart) {
  const QPointF moving = movingStart ? original.start : original.end;
  QPointF direction = moving - anchor;
  const qreal length = std::hypot(direction.x(), direction.y());
  if (length < 0.01)
    return point;
  direction /= length;
  const QPointF delta = point - anchor;
  const qreal projection = delta.x() * direction.x() +
                           delta.y() * direction.y();
  return anchor + direction * projection;
}
} // namespace

void SelectTool::reset() {
  mode_ = Mode::None;
  original_ = {};
  start_ = {};
  index_ = -1;
  handle_ = AnnotationHandle::None;
}

bool SelectTool::mousePress(const ToolEvent &event, ToolContext &context) {
  if (context.document.selected() >= 0 &&
      context.document.selected() < context.document.annotations().size()) {
    const Annotation &selected =
        context.document.annotations().at(context.document.selected());
    const AnnotationHandle handle = annotationHandleAt(selected, event.pos);
    if (handle != AnnotationHandle::None) {
      index_ = context.document.selected();
      original_ = selected;
      handle_ = handle;
      start_ = event.pos;
      mode_ = Mode::Resize;
      context.document.pushUndo();
      context.setStatus(
          ui("Resize · Shift keeps ratio · Ctrl resizes from center",
             "缩放 · Shift 等比例 · Ctrl 中心缩放"));
      return true;
    }
  }
  const int hit = annotationAt(context.document.annotations(), event.pos);
  if (hit < 0)
    return false;
  context.document.setSelected(hit);
  context.style.color = context.document.annotations().at(hit).color;
  context.style.width = context.document.annotations().at(hit).size;
  context.style.strokeStyle =
      context.document.annotations().at(hit).strokeStyle;
  context.style.filled = context.document.annotations().at(hit).filled;
  original_ = context.document.annotations().at(hit);
  index_ = hit;
  start_ = event.pos;
  handle_ = AnnotationHandle::None;
  mode_ = Mode::Move;
  context.document.pushUndo();
  return true;
}

bool SelectTool::mouseMove(const ToolEvent &event, ToolContext &context) {
  if (mode_ == Mode::None || index_ < 0 ||
      index_ >= context.document.annotations().size())
    return false;
  Annotation changed = original_;
  if (mode_ == Mode::Move) {
    changed = translatedAnnotation(original_, event.pos - start_);
  } else if (handle_ != AnnotationHandle::None) {
    const bool movingStart = handle_ == AnnotationHandle::Start;
    const QPointF center =
        (original_.start + original_.end) / 2.0;
    const bool fromCenter =
        event.modifiers.testFlag(Qt::ControlModifier);
    QPointF anchor = movingStart ? original_.end : original_.start;
    if (fromCenter)
      anchor = center;
    QPointF target = event.pos;
    if (event.modifiers.testFlag(Qt::ShiftModifier)) {
      if (original_.kind == Annotation::Kind::Line ||
          original_.kind == Annotation::Kind::Arrow) {
        target = angleConstrained(original_, anchor, target, movingStart);
      } else {
        target = aspectConstrained(original_, anchor, target);
      }
    }
    if (fromCenter) {
      changed.start = target;
      changed.end = center * 2.0 - target;
    } else if (movingStart) {
      changed.start = target;
    } else {
      changed.end = target;
    }
  }
  context.document.annotations()[index_] = std::move(changed);
  context.requestRepaint();
  return true;
}

bool SelectTool::mouseRelease(const ToolEvent &, ToolContext &) {
  const bool consumed = mode_ != Mode::None;
  reset();
  return consumed;
}

bool SelectTool::keyPress(QKeyEvent *event, ToolContext &context) {
  if (event->key() != Qt::Key_Delete && event->key() != Qt::Key_Backspace)
    return false;
  if (context.document.selected() < 0)
    return false;
  context.document.pushUndo();
  context.document.removeSelected();
  context.requestRepaint();
  return true;
}

bool SelectTool::wheel(QWheelEvent *event, ToolContext &context) {
  if (context.document.selected() < 0)
    return false;
  const qreal steps = event->angleDelta().y() / 120.0;
  if (qFuzzyIsNull(steps))
    return false;
  Annotation &annotation =
      context.document.annotations()[context.document.selected()];
  context.document.pushUndo();
  const qreal factor = steps > 0 ? 1.12 : 1.0 / 1.12;
  annotation.size = std::clamp(annotation.size * factor, 2.0, 96.0);
  context.style.width = annotation.size;
  context.requestRepaint();
  return true;
}
