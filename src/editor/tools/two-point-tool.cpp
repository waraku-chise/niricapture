#include "editor/tools/two-point-tool.hpp"

#include "editor/annotation-document.hpp"

#include <QLineF>
#include <QPainter>

void TwoPointTool::reset() {
  active_ = false;
  start_ = {};
  end_ = {};
}

bool TwoPointTool::mousePress(const ToolEvent &event, ToolContext &context) {
  if (!context.selection.contains(event.pos))
    return false;
  reset();
  active_ = true;
  const bool snap = static_cast<bool>(context.snapPoint);
  start_ = snap ? context.snapPoint(event.pos, nullptr, false) : event.pos;
  end_ = start_;
  previewColor_ = context.style.color;
  previewWidth_ = context.style.width;
  previewStrokeStyle_ = context.style.strokeStyle;
  previewFilled_ = supportsFill() && context.style.filled;
  context.requestRepaint();
  return true;
}

bool TwoPointTool::mouseMove(const ToolEvent &event, ToolContext &context) {
  if (!active_)
    return false;
  const bool angleSnap =
      (kind() == Annotation::Kind::Line || kind() == Annotation::Kind::Arrow);
  end_ = context.snapPoint ? context.snapPoint(event.pos, &start_, angleSnap)
                           : event.pos;
  context.requestRepaint();
  return true;
}

bool TwoPointTool::mouseRelease(const ToolEvent &event, ToolContext &context) {
  if (!active_)
    return false;
  const bool angleSnap =
      (kind() == Annotation::Kind::Line || kind() == Annotation::Kind::Arrow);
  end_ = context.snapPoint ? context.snapPoint(event.pos, &start_, angleSnap)
                           : event.pos;
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
