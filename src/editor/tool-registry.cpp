#include "editor/tool-registry.hpp"

#include <algorithm>

bool ToolRegistry::add(std::unique_ptr<IEditorTool> tool) {
  if (!tool || find(tool->id()))
    return false;
  tools_.push_back(std::move(tool));
  return true;
}

IEditorTool *ToolRegistry::find(const QString &id) const {
  for (const auto &tool : tools_) {
    if (tool->id() == id)
      return tool.get();
  }
  return nullptr;
}

QString ToolRegistry::defaultToolId() const {
  return tools_.empty() ? QString() : tools_.front()->id();
}
