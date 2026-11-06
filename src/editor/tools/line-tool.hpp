/** @fileoverview Line tool. */
#pragma once

#include "editor/tools/two-point-tool.hpp"
#include "i18n.hpp"

class LineTool final : public TwoPointTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("line"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Line"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-line"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("L"); }
  [[nodiscard]] QString tooltip() const override { return ui("Line", "直线"); }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Shape; }

protected:
  [[nodiscard]] Annotation::Kind kind() const override {
    return Annotation::Kind::Line;
  }
  [[nodiscard]] bool supportsFill() const override { return false; }
  [[nodiscard]] QString addedStatus() const override { return tooltip(); }
};
