/** @fileoverview Rectangle tool. */
#pragma once

#include "editor/tools/two-point-tool.hpp"
#include "i18n.hpp"

class RectangleTool final : public TwoPointTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("rectangle"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Rectangle"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-rectangle"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("R"); }
  [[nodiscard]] QString tooltip() const override { return ui("Rectangle", "矩形"); }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Shape; }

protected:
  [[nodiscard]] Annotation::Kind kind() const override {
    return Annotation::Kind::Rectangle;
  }
  [[nodiscard]] bool supportsFill() const override { return true; }
  [[nodiscard]] QString addedStatus() const override { return tooltip(); }
};
