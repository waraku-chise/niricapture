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
  const QStringList ids = visibleToolIds();
  return ids.isEmpty() ? QString() : ids.first();
}

void ToolRegistry::setVisibleTools(const QStringList &ids) {
  visibleIds_.clear();
  for (const QString &id : ids) {
    if (find(id) && !visibleIds_.contains(id))
      visibleIds_.push_back(id);
  }
}

QStringList ToolRegistry::visibleToolIds() const {
  if (!visibleIds_.isEmpty())
    return visibleIds_;
  QStringList ids;
  for (const auto &tool : tools_)
    ids.push_back(tool->id());
  return ids;
}

bool ToolRegistry::visible(const QString &id) const {
  return visibleToolIds().contains(id);
}
