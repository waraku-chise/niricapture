/** @fileoverview Translucent freehand highlighter tool. */
#pragma once

#include "editor/tools/freehand-tool.hpp"
#include "i18n.hpp"

class HighlighterTool final : public FreehandTool {
public:
  [[nodiscard]] QString id() const override {
    return QStringLiteral("highlighter");
  }
  [[nodiscard]] QString name() const override {
    return QStringLiteral("Highlighter");
  }
  [[nodiscard]] QString icon() const override {
    return QStringLiteral("tool-highlighter");
  }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("H"); }
  [[nodiscard]] QString tooltip() const override { return ui("Highlighter", "高亮"); }

protected:
  [[nodiscard]] Annotation::Kind kind() const override {
    return Annotation::Kind::Highlighter;
  }
  [[nodiscard]] QColor color(const EditorStyle &style) const override {
    QColor result = style.color;
    result.setAlpha(110);
    return result;
  }
  [[nodiscard]] QString addedStatus() const override { return tooltip(); }
};
