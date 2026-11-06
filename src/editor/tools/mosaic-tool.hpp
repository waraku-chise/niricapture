/** @fileoverview Pixelating redaction tool. */
#pragma once

#include "editor/tools/two-point-tool.hpp"
#include "i18n.hpp"

class MosaicTool final : public TwoPointTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("mosaic"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Mosaic"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-redact"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("D"); }
  [[nodiscard]] QString tooltip() const override { return ui("Mosaic", "马赛克"); }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Effect; }
  bool mouseRelease(const ToolEvent &, ToolContext &) override;
  [[nodiscard]] std::optional<Annotation> preview() const override;

protected:
  [[nodiscard]] Annotation::Kind kind() const override {
    return Annotation::Kind::Redaction;
  }
  [[nodiscard]] QString addedStatus() const override { return tooltip(); }
};
