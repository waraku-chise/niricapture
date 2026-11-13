#include "editor/tools/two-point-tool.hpp"

#include "editor/annotation-document.hpp"
#include "i18n.hpp"

#include <QLineF>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kPi = 3.14159265358979323846;

QPointF aspectTarget(const QPointF &anchor, const QPointF &point) {
  const qreal dx = point.x() - anchor.x();
  const qreal dy = point.y() - anchor.y();
  const qreal side = std::max(std::abs(dx), std::abs(dy));
  return {anchor.x() + (dx < 0 ? -side : side),
          anchor.y() + (dy < 0 ? -side : side)};
}

QPointF angleTarget(const QPointF &anchor, const QPointF &point) {
  const QPointF delta = point - anchor;
  const qreal length = std::hypot(delta.x(), delta.y());
  if (length < 0.01)
    return point;
  constexpr qreal step = kPi / 4.0;
  const qreal angle =
      std::round(std::atan2(delta.y(), delta.x()) / step) * step;
  const QPointF direction(std::cos(angle), std::sin(angle));
  const qreal projection =
      delta.x() * direction.x() + delta.y() * direction.y();
  return anchor + direction * projection;
}
} // namespace

void TwoPointTool::reset() {
  active_ = false;
  start_ = {};
  end_ = {};
  center_ = {};
}

bool TwoPointTool::mousePress(const ToolEvent &event, ToolContext &context) {
  if (!context.selection.contains(event.pos))
    return false;
  reset();
  active_ = true;
  const bool snap = static_cast<bool>(context.snapPoint);
  center_ = snap ? context.snapPoint(event.pos, nullptr, false) : event.pos;
  start_ = center_;
  end_ = center_;
  previewColor_ = context.style.color;
  previewWidth_ = context.style.width;
  previewStrokeStyle_ = context.style.strokeStyle;
  previewFilled_ = supportsFill() && context.style.filled;
  const bool lineLike =
      kind() == Annotation::Kind::Line || kind() == Annotation::Kind::Arrow;
  context.setStatus(
      lineLike ? ui("Create · Shift 45° · Ctrl draws from centre",
                    "创建 · Shift 45° · Ctrl 中心缩放")
               : ui("Create · Shift keeps ratio · Ctrl draws from centre",
                    "创建 · Shift 等比例 · Ctrl 中心缩放"));
  context.requestRepaint();
  return true;
}

void TwoPointTool::updateDrag(const QPointF &point,
                              Qt::KeyboardModifiers modifiers,
                              ToolContext &context) {
  const bool lineLike =
      kind() == Annotation::Kind::Line || kind() == Annotation::Kind::Arrow;
  const QPointF anchor = center_;
  QPointF target =
      context.snapPoint
          ? context.snapPoint(point, lineLike ? &anchor : nullptr, lineLike)
          : point;
  if (modifiers.testFlag(Qt::ShiftModifier))
    target = lineLike ? angleTarget(anchor, target)
                      : aspectTarget(anchor, target);
  if (modifiers.testFlag(Qt::ControlModifier)) {
    start_ = anchor * 2.0 - target;
    end_ = target;
  } else {
    start_ = anchor;
    end_ = target;
  }
  context.requestRepaint();
}

bool TwoPointTool::mouseMove(const ToolEvent &event, ToolContext &context) {
  if (!active_)
    return false;
  updateDrag(event.pos, event.modifiers, context);
  return true;
}

bool TwoPointTool::mouseRelease(const ToolEvent &event, ToolContext &context) {
  if (!active_)
    return false;
  updateDrag(event.pos, event.modifiers, context);
  active_ = false;
  const bool valid = QLineF(start_, end_).length() > 4.0;
  if (valid) {
    context.document.pushUndo();
    Annotation annotation;
    annotation.kind = kind();
    annotation.start = start_;
    annotation.end = end_;
    annotation.color = context.style.color;
    annotation.size = context.style.width;
    annotation.strokeStyle = context.style.strokeStyle;
    annotation.filled = supportsFill() && context.style.filled;
    context.document.add(std::move(annotation));
    context.setStatus(addedStatus());
  }
  reset();
  context.requestRepaint();
  return true;
}

std::optional<Annotation> TwoPointTool::preview() const {
  if (!active_)
    return std::nullopt;
  Annotation annotation;
  annotation.kind = kind();
  annotation.start = start_;
  annotation.end = end_;
  annotation.color = previewColor_;
  annotation.size = previewWidth_;
  annotation.strokeStyle = previewStrokeStyle_;
  annotation.filled = previewFilled_;
  return annotation;
}

void TwoPointTool::paintPreview(QPainter &, const ToolContext &) const {}
