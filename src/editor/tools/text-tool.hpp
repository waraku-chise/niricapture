/** @fileoverview Text insertion tool. */
#pragma once

#include "editor/tool.hpp"
#include "i18n.hpp"

class TextTool final : public IEditorTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("text"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Text"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-text"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("T"); }
  [[nodiscard]] QString tooltip() const override { return ui("Text", "文字"); }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Text; }
  void reset() override {}
  bool mousePress(const ToolEvent &, ToolContext &) override;
};
