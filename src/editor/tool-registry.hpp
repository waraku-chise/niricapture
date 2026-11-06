/** @fileoverview Owns tool instances and resolves tools by stable id. */
#pragma once

#include "editor/tool.hpp"

#include <QString>

#include <memory>
#include <vector>
#include <QStringList>

class ToolRegistry {
public:
  ToolRegistry() = default;
  ToolRegistry(const ToolRegistry &) = delete;
  ToolRegistry &operator=(const ToolRegistry &) = delete;

  /** Takes ownership; returns false for a duplicate id. */
  bool add(std::unique_ptr<IEditorTool> tool);
  template <typename T, typename... Args> T *emplace(Args &&...args) {
    auto tool = std::make_unique<T>(std::forward<Args>(args)...);
    T *raw = tool.get();
    if (!add(std::move(tool)))
      return nullptr;
    return raw;
  }

  [[nodiscard]] IEditorTool *find(const QString &id) const;
  [[nodiscard]] const std::vector<std::unique_ptr<IEditorTool>> &tools() const {
    return tools_;
  }
  [[nodiscard]] QString defaultToolId() const;
  /** Ordered, visible tool ids. Empty means all registered tools. */
  void setVisibleTools(const QStringList &ids);
  [[nodiscard]] QStringList visibleToolIds() const;
  [[nodiscard]] bool visible(const QString &id) const;

private:
  std::vector<std::unique_ptr<IEditorTool>> tools_;
  QStringList visibleIds_;
};

/** Registers the built-in inline tools. New tools are added here only. */
void registerBuiltinTools(ToolRegistry &registry);
