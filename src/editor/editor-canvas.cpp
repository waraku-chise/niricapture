#include "editor/editor-canvas.hpp"

#include "editor/annotation-geometry.hpp"
#include "editor/tools/builtin-tools.hpp"

#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

EditorCanvas::EditorCanvas() {
  setStatus_ = [](const QString &) {};
  repaint_ = [] {};
  beginText_ = [](const QPointF &) {};
  registerBuiltinTools(registry_);
  currentToolId_ = registry_.defaultToolId();
  if (IEditorTool *tool = currentTool()) {
    ToolContext ctx = context();
    tool->activate(ctx);
  }
}

EditorCanvas::~EditorCanvas() = default;

void EditorCanvas::setSelection(const QRectF &selection) {
  selection_ = selection.normalized();
}

QString EditorCanvas::currentToolId() const { return currentToolId_; }

IEditorTool *EditorCanvas::currentTool() const {
  return registry_.find(currentToolId_);
}

void EditorCanvas::setTool(const QString &id) {
  if (id == currentToolId_ || !registry_.find(id))
    return;
  ToolContext ctx = context();
  if (IEditorTool *old = currentTool()) {
    old->deactivate(ctx);
    old->reset();
  }
  active_ = false;
  currentToolId_ = id;
  if (IEditorTool *tool = currentTool()) {
    tool->reset();
    tool->activate(ctx);
  }
  if (repaint_)
    repaint_();
  if (setStatus_ && currentTool())
    setStatus_(currentTool()->tooltip());
}

void EditorCanvas::setStatusCallback(
    std::function<void(const QString &)> callback) {
  setStatus_ = std::move(callback);
}

void EditorCanvas::setRepaintCallback(std::function<void()> callback) {
  repaint_ = std::move(callback);
}

void EditorCanvas::setTextCallback(
    std::function<void(const QPointF &)> callback) {
  beginText_ = std::move(callback);
}

ToolContext EditorCanvas::context() {
  return ToolContext{document_,    selection_, style_,   setStatus_,
                     repaint_,     beginText_};
}

bool EditorCanvas::press(const ToolEvent &event) {
  IEditorTool *tool = currentTool();
  if (!tool)
    return false;
  ToolContext ctx = context();
  active_ = tool->mousePress(event, ctx);
  return active_;
}

bool EditorCanvas::move(const ToolEvent &event) {
  if (!active_)
    return false;
  IEditorTool *tool = currentTool();
  if (!tool)
    return false;
  ToolContext ctx = context();
  return tool->mouseMove(event, ctx);
}

bool EditorCanvas::release(const ToolEvent &event) {
  if (!active_)
    return false;
  IEditorTool *tool = currentTool();
  if (!tool)
    return false;
  ToolContext ctx = context();
  const bool consumed = tool->mouseRelease(event, ctx);
  active_ = false;
  return consumed;
}

bool EditorCanvas::keyPress(QKeyEvent *event) {
  if (IEditorTool *tool = currentTool()) {
    ToolContext ctx = context();
    if (tool->keyPress(event, ctx))
      return true;
  }
  if (event->matches(QKeySequence::Undo)) {
    document_.undo();
    if (repaint_)
      repaint_();
    return true;
  }
  if (event->matches(QKeySequence::Redo)) {
    document_.redo();
    if (repaint_)
      repaint_();
    return true;
  }
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    if (document_.selected() < 0)
      return false;
    document_.pushUndo();
    document_.removeSelected();
    if (repaint_)
      repaint_();
    return true;
  }
  if (!event->modifiers().testFlag(Qt::ControlModifier) &&
      !event->modifiers().testFlag(Qt::AltModifier) &&
      !event->modifiers().testFlag(Qt::MetaModifier)) {
    for (const auto &tool : registry_.tools()) {
      if (event->key() == QKeySequence(tool->shortcut())[0].key()) {
        setTool(tool->id());
        return true;
      }
    }
  }
  return false;
}

bool EditorCanvas::wheel(QWheelEvent *event) {
  IEditorTool *tool = currentTool();
  if (!tool)
    return false;
  ToolContext ctx = context();
  if (tool->wheel(event, ctx)) {
    if (repaint_)
      repaint_();
    return true;
  }
  return false;
}

std::optional<Annotation> EditorCanvas::preview() const {
  IEditorTool *tool = currentTool();
  return tool ? tool->preview() : std::nullopt;
}

void EditorCanvas::resetTool() {
  if (IEditorTool *tool = currentTool())
    tool->reset();
  active_ = false;
}

void EditorCanvas::paintVectors(QPainter &painter, const QPointF &origin) const {
  painter.save();
  painter.translate(-origin.x(), -origin.y());
  painter.setClipRect(selection_);
  for (const Annotation &annotation : document_.annotations()) {
    if (annotationLayer(annotation.kind) == AnnotationLayer::Redaction)
      continue;
    paintAnnotation(painter, annotation);
  }
  if (const auto inProgress = preview()) {
    if (annotationLayer(inProgress->kind) == AnnotationLayer::Default)
      paintAnnotation(painter, *inProgress);
  }
  if (document_.selected() >= 0 &&
      document_.selected() < document_.annotations().size()) {
    const Annotation &selected =
        document_.annotations().at(document_.selected());
    if (annotationSupportsEndpoints(selected)) {
      painter.setPen(QPen(QColor(124, 196, 255), 1.5, Qt::DashLine));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(QRectF(selected.start, selected.end).normalized());
      painter.setBrush(QColor(124, 196, 255));
      painter.setPen(Qt::NoPen);
      painter.drawEllipse(selected.start, 4, 4);
      painter.drawEllipse(selected.end, 4, 4);
    }
  }
  painter.restore();
}
