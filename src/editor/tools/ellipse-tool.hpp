/** @fileoverview Ellipse tool. */
#pragma once

#include "editor/tools/two-point-tool.hpp"
#include "i18n.hpp"

class EllipseTool final : public TwoPointTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("ellipse"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Ellipse"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-ellipse"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("E"); }
  [[nodiscard]] QString tooltip() const override { return ui("Ellipse", "椭圆"); }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Shape; }

protected:
  [[nodiscard]] Annotation::Kind kind() const override {
    return Annotation::Kind::Ellipse;
  }
  [[nodiscard]] bool supportsFill() const override { return true; }
  [[nodiscard]] QString addedStatus() const override { return tooltip(); }
};
