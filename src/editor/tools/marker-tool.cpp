#include "editor/tools/marker-tool.hpp"

#include "editor/annotation-document.hpp"
#include "i18n.hpp"

bool MarkerTool::mousePress(const ToolEvent &event, ToolContext &context) {
  if (!context.selection.contains(event.pos))
    return false;
  context.document.pushUndo();
  Annotation annotation;
  annotation.kind = Annotation::Kind::Marker;
  annotation.start = event.pos;
  annotation.number = context.document.consumeMarker();
  annotation.color = context.style.color;
  annotation.size = std::max<qreal>(12.0, context.style.width * 2.0);
  const int number = annotation.number;
  context.document.add(std::move(annotation));
  context.setStatus(ui("Number %1", "序号 %1").arg(number));
  context.requestRepaint();
  return true;
}
