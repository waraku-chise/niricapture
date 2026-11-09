/** @fileoverview Event dispatch and annotation painting for the editor. */
#pragma once

#include "editor/annotation-document.hpp"
#include "editor/snap.hpp"
#include "editor/tool-registry.hpp"

#include <QPointF>
#include <QRectF>
#include <QHash>
#include <QString>

#include <functional>
#include <optional>

class QPainter;

/**
 * Owns tool state, the annotation document and the current selection. It does
 * not know about layer-shell surfaces or windows; callers translate events and
 * paint into their own viewport.
 */
class EditorCanvas {
public:
  EditorCanvas();
  ~EditorCanvas();

  [[nodiscard]] AnnotationDocument &document() { return document_; }
  [[nodiscard]] const AnnotationDocument &document() const { return document_; }
  [[nodiscard]] ToolRegistry &registry() { return registry_; }
  [[nodiscard]] const ToolRegistry &registry() const { return registry_; }
  [[nodiscard]] EditorStyle &style() { return style_; }
  [[nodiscard]] const EditorStyle &style() const { return style_; }
  void setDefaultStyle(const EditorStyle &style);
  void setToolShortcuts(const QHash<QString, QString> &shortcuts);
  void setWidthOptions(const QVector<int> &widths);
  [[nodiscard]] bool snapEnabled() const { return snapEnabled_; }
  void setSnapEnabled(bool enabled);
  void setColorSampler(std::function<QColor(const QPointF &)> sampler);

  void setSelection(const QRectF &selection);
  [[nodiscard]] QRectF selection() const { return selection_; }

  [[nodiscard]] QString currentToolId() const;
  void setTool(const QString &id);
  [[nodiscard]] IEditorTool *currentTool() const;

  void setStatusCallback(std::function<void(const QString &)> callback);
  void setRepaintCallback(std::function<void()> callback);
  void setTextCallback(std::function<void(const QPointF &)> callback);

  bool press(const ToolEvent &event);
  bool move(const ToolEvent &event);
  bool release(const ToolEvent &event);
  bool keyPress(QKeyEvent *event);
  bool wheel(QWheelEvent *event);

  [[nodiscard]] std::optional<Annotation> preview() const;
  [[nodiscard]] bool hasPreview() const { return preview().has_value(); }
  void resetTool();

  /** Paints vector annotations and the live preview (not pixel effects). */
  void paintVectors(QPainter &painter, const QPointF &origin) const;

private:
  [[nodiscard]] ToolContext context();
  [[nodiscard]] SnapResult snapPoint(const QPointF &point,
                                     const QPointF *anchor, bool angle) const;
  [[nodiscard]] TranslationSnap snapTranslation(
      const QRectF &bounds, const QPointF &delta) const;

  AnnotationDocument document_;
  ToolRegistry registry_;
  EditorStyle style_;
  QRectF selection_;
  QString currentToolId_;
  bool active_ = false;
  std::function<void(const QString &)> setStatus_;
  std::function<void()> repaint_;
  std::function<void(const QPointF &)> beginText_;
  QHash<QString, QString> shortcuts_;
  QVector<int> widthOptions_{2, 4, 8, 12};
  bool snapEnabled_ = true;
  std::function<QColor(const QPointF &)> sampleColor_;
  mutable QVector<QLineF> guides_;
};
