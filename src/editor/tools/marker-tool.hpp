/** @fileoverview Numbered marker tool. */
#pragma once

#include "editor/tool.hpp"
#include "i18n.hpp"

class MarkerTool final : public IEditorTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("marker"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Marker"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-marker"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("C"); }
  [[nodiscard]] QString tooltip() const override { return ui("Number marker", "序号"); }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Marker; }
  void reset() override {}
  bool mousePress(const ToolEvent &, ToolContext &) override;
};
