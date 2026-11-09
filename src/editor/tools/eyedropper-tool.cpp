#include "editor/tools/eyedropper-tool.hpp"

#include "i18n.hpp"

bool EyedropperTool::mousePress(const ToolEvent &event, ToolContext &context) {
  if (!context.selection.contains(event.pos) || !context.sampleColor)
    return false;
  const QColor color = context.sampleColor(event.pos);
  if (!color.isValid())
    return false;
  context.style.color = color;
  context.setStatus(ui("Sampled %1", "已取样 %1")
                         .arg(color.name(QColor::HexRgb).toUpper()));
  context.requestRepaint();
  return true;
}
