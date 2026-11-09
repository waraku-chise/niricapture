#include "editor/tools/freehand-tool.hpp"

#include "editor/annotation-document.hpp"

#include <QLineF>
#include <QPainter>

void FreehandTool::reset() {
  active_ = false;
  points_.clear();
}

bool FreehandTool::mousePress(const ToolEvent &event, ToolContext &context) {
  if (!context.selection.contains(event.pos))
    return false;
  reset();
  active_ = true;
  points_.push_back(event.pos);
  previewColor_ = color(context.style);
  previewWidth_ = context.style.width;
  previewStrokeStyle_ = context.style.strokeStyle;
  context.requestRepaint();
  return true;
}

bool FreehandTool::mouseMove(const ToolEvent &event, ToolContext &context) {
  if (!active_)
    return false;
  if (points_.isEmpty() ||
      QLineF(points_.constLast(), event.pos).length() >= 1.0)
    points_.push_back(event.pos);
  context.requestRepaint();
  return true;
}

bool FreehandTool::mouseRelease(const ToolEvent &event, ToolContext &context) {
  if (!active_)
    return false;
  if (points_.isEmpty() ||
      QLineF(points_.constLast(), event.pos).length() >= 1.0)
    points_.push_back(event.pos);
  qreal length = 0;
  for (int index = 1; index < points_.size(); ++index)
    length += QLineF(points_.at(index - 1), points_.at(index)).length();
  active_ = false;
  if (length > 4.0) {
    context.document.pushUndo();
    Annotation annotation;
    annotation.kind = kind();
    annotation.start = points_.first();
    annotation.end = points_.last();
    annotation.color = color(context.style);
    annotation.size = context.style.width;
    annotation.strokeStyle = context.style.strokeStyle;
    annotation.points = points_;
    context.document.add(std::move(annotation));
    context.setStatus(addedStatus());
  }
  reset();
  context.requestRepaint();
  return true;
}

std::optional<Annotation> FreehandTool::preview() const {
  if (!active_ || points_.isEmpty())
    return std::nullopt;
  Annotation annotation;
  annotation.kind = kind();
  annotation.start = points_.first();
  annotation.end = points_.last();
  annotation.color = previewColor_;
  annotation.size = previewWidth_;
  annotation.strokeStyle = previewStrokeStyle_;
  annotation.points = points_;
  return annotation;
}

void FreehandTool::paintPreview(QPainter &, const ToolContext &) const {}
