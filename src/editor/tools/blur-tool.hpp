/** @fileoverview Gaussian blur tool. */
#pragma once

#include "editor/tools/two-point-tool.hpp"
#include "i18n.hpp"

class BlurTool final : public TwoPointTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("blur"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Blur"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-blur"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("B"); }
  [[nodiscard]] QString tooltip() const override { return ui("Blur", "模糊"); }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Effect; }
  void activate(ToolContext &context) override;

protected:
  [[nodiscard]] Annotation::Kind kind() const override {
    return Annotation::Kind::Blur;
  }
  [[nodiscard]] QString addedStatus() const override { return tooltip(); }
};
