/** @fileoverview Common interface for editor tools and their event context. */
#pragma once

#include "capture.hpp"

#include <QColor>
#include <QPointF>
#include <QString>

#include <functional>
#include <optional>

class QPainter;
class QKeyEvent;
class QWheelEvent;
class AnnotationDocument;

/** Shared drawing style mutated by the palette / toolbar. */
struct EditorStyle {
  QColor color = QColor(QStringLiteral("#ff375f"));
  qreal width = 4.0;
  QString fontFamily = QStringLiteral("Neucha");
  bool filled = false;
};

/** Per-event data. Coordinates are global logical desktop coordinates. */
struct ToolEvent {
  QPointF pos;
  QPointF local;
  Qt::KeyboardModifiers modifiers = Qt::NoModifier;
  Qt::MouseButton button = Qt::LeftButton;
};

/**
 * Services a tool may use. The context is valid only for the duration of the
 * callback; tools must not retain references to it.
 */
struct ToolContext {
  AnnotationDocument &document;
  const QRectF &selection;
  EditorStyle &style;
  std::function<void(const QString &)> setStatus;
  std::function<void()> requestRepaint;
  std::function<void(const QPointF &)> beginText;
  std::function<QColor(const QPointF &)> sampleColor;
};

enum class ToolGroup { Select, Draw, Shape, Text, Effect, Marker, Utility };

/**
 * One editor tool. Drawing tools own their temporary interaction state and
 * return their preview as a normal Annotation so undo/export see one model.
 */
class IEditorTool {
public:
  virtual ~IEditorTool() = default;

  [[nodiscard]] virtual QString id() const = 0;
  [[nodiscard]] virtual QString name() const = 0;
  [[nodiscard]] virtual QString icon() const = 0;
  [[nodiscard]] virtual QString shortcut() const = 0;
  [[nodiscard]] virtual QString tooltip() const = 0;
  [[nodiscard]] virtual ToolGroup group() const = 0;

  virtual void activate(ToolContext &) {}
  virtual void deactivate(ToolContext &) {}
  /** Clears any in-flight state. Must be safe to call repeatedly. */
  virtual void reset() = 0;

  virtual bool mousePress(const ToolEvent &, ToolContext &) { return false; }
  virtual bool mouseMove(const ToolEvent &, ToolContext &) { return false; }
  virtual bool mouseRelease(const ToolEvent &, ToolContext &) { return false; }
  virtual bool keyPress(QKeyEvent *, ToolContext &) { return false; }
  virtual bool wheel(QWheelEvent *, ToolContext &) { return false; }

  /** In-progress annotation, if any. Never stored in the document. */
  [[nodiscard]] virtual std::optional<Annotation> preview() const {
    return std::nullopt;
  }
  virtual void paintPreview(QPainter &, const ToolContext &) const {}
};
