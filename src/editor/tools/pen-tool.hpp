/** @fileoverview Freehand pen tool. */
#pragma once

#include "editor/tools/freehand-tool.hpp"
#include "i18n.hpp"

class PenTool final : public FreehandTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("pen"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Pen"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-freehand"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("F"); }
  [[nodiscard]] QString tooltip() const override { return ui("Pen", "画笔"); }

protected:
  [[nodiscard]] Annotation::Kind kind() const override {
    return Annotation::Kind::Freehand;
  }
  [[nodiscard]] QColor color(const EditorStyle &style) const override {
    return style.color;
  }
  [[nodiscard]] QString addedStatus() const override { return tooltip(); }
};
