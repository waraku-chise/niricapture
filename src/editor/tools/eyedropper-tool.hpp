/** @fileoverview Samples a colour from the source image. */
#pragma once

#include "editor/tool.hpp"
#include "i18n.hpp"

class EyedropperTool final : public IEditorTool {
public:
  [[nodiscard]] QString id() const override {
    return QStringLiteral("eyedropper");
  }
  [[nodiscard]] QString name() const override {
    return QStringLiteral("Eyedropper");
  }
  [[nodiscard]] QString icon() const override {
    return QStringLiteral("tool-eyedropper");
  }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("I"); }
  [[nodiscard]] QString tooltip() const override {
    return ui("Eyedropper · sample colour", "取色器 · 从图像取色");
  }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Utility; }
  void reset() override {}
  bool mousePress(const ToolEvent &, ToolContext &) override;
};
