/** @fileoverview Base for drag-created annotation tools. */
#pragma once

#include "editor/tool.hpp"

class TwoPointTool : public IEditorTool {
public:
  [[nodiscard]] ToolGroup group() const override { return ToolGroup::Shape; }
  void reset() override;
  bool mousePress(const ToolEvent &, ToolContext &) override;
  bool mouseMove(const ToolEvent &, ToolContext &) override;
  bool mouseRelease(const ToolEvent &, ToolContext &) override;
  [[nodiscard]] std::optional<Annotation> preview() const override;
  void paintPreview(QPainter &, const ToolContext &) const override;

protected:
  [[nodiscard]] virtual Annotation::Kind kind() const = 0;
  [[nodiscard]] virtual bool supportsFill() const { return false; }
  [[nodiscard]] virtual QString addedStatus() const = 0;

  void updateDrag(const QPointF &point, Qt::KeyboardModifiers modifiers,
                  ToolContext &context);

  QPointF start_;
  QPointF end_;
  QPointF center_;
  QColor previewColor_;
  qreal previewWidth_ = 4.0;
  StrokeStyle previewStrokeStyle_ = StrokeStyle::Solid;
  bool previewFilled_ = false;
  bool active_ = false;
};
