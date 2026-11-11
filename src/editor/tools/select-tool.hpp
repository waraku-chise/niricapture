/** @fileoverview Selection, move and endpoint-resize tool. */
#pragma once

#include "editor/annotation-geometry.hpp"
#include "editor/tool.hpp"
#include "i18n.hpp"

class SelectTool final : public IEditorTool {
public:
  [[nodiscard]] QString id() const override { return QStringLiteral("select"); }
  [[nodiscard]] QString name() const override { return QStringLiteral("Select"); }
  [[nodiscard]] QString icon() const override { return QStringLiteral("tool-select"); }
  [[nodiscard]] QString shortcut() const override { return QStringLiteral("V"); }
  [[nodiscard]] QString tooltip() const override {
    return ui("Select / move", "选择 · 移动 / 缩放");
  }
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Select; }
  void reset() override;
  bool mousePress(const ToolEvent &, ToolContext &) override;
  bool mouseMove(const ToolEvent &, ToolContext &) override;
  bool mouseRelease(const ToolEvent &, ToolContext &) override;
  bool keyPress(QKeyEvent *, ToolContext &) override;
  bool wheel(QWheelEvent *, ToolContext &) override;

private:
  enum class Mode { None, Move, Resize };
  Mode mode_ = Mode::None;
  Annotation original_;
  QPointF start_;
  int index_ = -1;
  AnnotationHandle handle_ = AnnotationHandle::None;
};
