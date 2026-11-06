/** @fileoverview Arrow tool. */
#pragma once

#include "editor/tools/two-point-tool.hpp"
#include "i18n.hpp"

class ArrowTool final : public TwoPointTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("arrow"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Arrow"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-arrow"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("A"); }
  [[nodiscard]] QString tooltip() const override { return ui("Arrow", "箭头"); }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Shape; }

protected:
  [[nodiscard]] Annotation::Kind kind() const override {
    return Annotation::Kind::Arrow;
  }
  [[nodiscard]] bool supportsFill() const override { return false; }
  [[nodiscard]] QString addedStatus() const override { return tooltip(); }
};
