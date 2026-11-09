/** @fileoverview Base for freehand pen and highlighter tools. */
#pragma once

#include "editor/tool.hpp"

class FreehandTool : public IEditorTool {
public:
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Draw; }
  void reset() override;
  bool mousePress(const ToolEvent &, ToolContext &) override;
  bool mouseMove(const ToolEvent &, ToolContext &) override;
  bool mouseRelease(const ToolEvent &, ToolContext &) override;
  [[nodiscard]] std::optional<Annotation> preview() const override;
  void paintPreview(QPainter &, const ToolContext &) const override;

protected:
  [[nodiscard]] virtual Annotation::Kind kind() const = 0;
  [[nodiscard]] virtual QColor color(const EditorStyle &style) const = 0;
  [[nodiscard]] virtual QString addedStatus() const = 0;

  QVector<QPointF> points_;
  QColor previewColor_;
  qreal previewWidth_ = 4.0;
  StrokeStyle previewStrokeStyle_ = StrokeStyle::Solid;
  bool active_ = false;
};
