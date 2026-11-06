#include "editor/tools/blur-tool.hpp"

void BlurTool::activate(ToolContext &context) {
  if (context.style.width < 8)
    context.style.width = 16;
}
