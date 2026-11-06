#include "editor/tools/text-tool.hpp"

bool TextTool::mousePress(const ToolEvent &event, ToolContext &context) {
  if (!context.selection.contains(event.pos))
    return false;
  context.beginText(event.pos);
  context.setStatus(ui("Text · Enter to commit · Esc to cancel",
                       "文字 · Enter 提交 · Esc 取消"));
  return true;
}
