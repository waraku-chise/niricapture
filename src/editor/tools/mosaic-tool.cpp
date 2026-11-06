#include "editor/tools/mosaic-tool.hpp"

#include "editor/annotation-document.hpp"

#include <QLineF>

bool MosaicTool::mouseRelease(const ToolEvent &event, ToolContext &context) {
  if (!active_)
    return false;
  end_ = event.pos;
  active_ = false;
  const bool valid = QLineF(start_, end_).length() > 4.0;
  if (valid) {
    context.document.pushUndo();
    Annotation annotation;
    annotation.kind = Annotation::Kind::Redaction;
    annotation.redactionStyle = RedactionStyle::Pixelate;
    annotation.redactionSeed = 1;
    annotation.start = start_;
    annotation.end = end_;
    annotation.color = context.style.color;
    annotation.size = context.style.width;
    context.document.add(std::move(annotation));
    context.setStatus(addedStatus());
  }
  reset();
  context.requestRepaint();
  return true;
}

std::optional<Annotation> MosaicTool::preview() const {
  if (!active_)
    return std::nullopt;
  Annotation annotation;
  annotation.kind = Annotation::Kind::Redaction;
  annotation.redactionStyle = RedactionStyle::Pixelate;
  annotation.redactionSeed = 1;
  annotation.start = start_;
  annotation.end = end_;
  annotation.color = previewColor_;
  annotation.size = previewWidth_;
  return annotation;
}
