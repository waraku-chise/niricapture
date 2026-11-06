#include "editor/tools/builtin-tools.hpp"

#include "editor/tool-registry.hpp"
#include "editor/tools/arrow-tool.hpp"
#include "editor/tools/blur-tool.hpp"
#include "editor/tools/ellipse-tool.hpp"
#include "editor/tools/highlighter-tool.hpp"
#include "editor/tools/line-tool.hpp"
#include "editor/tools/marker-tool.hpp"
#include "editor/tools/mosaic-tool.hpp"
#include "editor/tools/pen-tool.hpp"
#include "editor/tools/rectangle-tool.hpp"
#include "editor/tools/select-tool.hpp"
#include "editor/tools/text-tool.hpp"

void registerBuiltinTools(ToolRegistry &registry) {
  registry.emplace<SelectTool>();
  registry.emplace<PenTool>();
  registry.emplace<ArrowTool>();
  registry.emplace<LineTool>();
  registry.emplace<RectangleTool>();
  registry.emplace<EllipseTool>();
  registry.emplace<TextTool>();
  registry.emplace<HighlighterTool>();
  registry.emplace<MosaicTool>();
  registry.emplace<BlurTool>();
  registry.emplace<MarkerTool>();
}
