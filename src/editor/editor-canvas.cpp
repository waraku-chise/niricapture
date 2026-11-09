#include "editor/editor-canvas.hpp"
#include "i18n.hpp"

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
  sampleColor_ = [](const QPointF &) { return QColor(); };
  registerBuiltinTools(registry_);
  currentToolId_ = registry_.defaultToolId();
  if (IEditorTool *tool = currentTool()) {
    ToolContext ctx = context();
    tool->activate(ctx);
  }
}

EditorCanvas::~EditorCanvas() = default;

void EditorCanvas::setDefaultStyle(const EditorStyle &style) {
  style_ = style;
  if (repaint_)
    repaint_();
}

void EditorCanvas::setToolShortcuts(
    const QHash<QString, QString> &shortcuts) {
  shortcuts_ = shortcuts;
}

void EditorCanvas::setWidthOptions(const QVector<int> &widths) {
  if (!widths.isEmpty())
    widthOptions_ = widths;
}

void EditorCanvas::setSnapEnabled(bool enabled) {
  snapEnabled_ = enabled;
  guides_.clear();
  if (repaint_)
    repaint_();
}

void EditorCanvas::setColorSampler(
    std::function<QColor(const QPointF &)> sampler) {
  sampleColor_ = std::move(sampler);
}

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
  return ToolContext{
      document_, selection_, style_, setStatus_, repaint_, beginText_,
      sampleColor_,
      [this](const QPointF &point, const QPointF *anchor, bool angle) {
        if (!snapEnabled_) {
          guides_.clear();
          return point;
        }
        const SnapResult result = snapPoint(point, anchor, angle);
        guides_ = result.guides;
        return result.point;
      },
      [this](const QRectF &bounds, const QPointF &delta) {
        if (!snapEnabled_) {
          guides_.clear();
          return delta;
        }
        const TranslationSnap result = snapTranslation(bounds, delta);
        guides_ = result.guides;
        return result.delta;
      }};
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
  guides_.clear();
  return consumed;
}

bool EditorCanvas::keyPress(QKeyEvent *event) {
  if (event->key() == Qt::Key_Alt && !event->isAutoRepeat()) {
    snapEnabled_ = !snapEnabled_;
    guides_.clear();
    if (setStatus_)
      setStatus_(snapEnabled_ ? ui("Snapping: on", "磁吸：开")
                              : ui("Snapping: off", "磁吸：关"));
    if (repaint_)
      repaint_();
    return true;
  }
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
    for (const QString &id : registry_.visibleToolIds()) {
      IEditorTool *tool = registry_.find(id);
      if (!tool)
        continue;
      const QString sequence = shortcuts_.value(id, tool->shortcut());
      if (!sequence.isEmpty() &&
          event->key() == QKeySequence(sequence)[0].key()) {
        setTool(id);
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
  if (widthOptions_.isEmpty() ||
      event->modifiers().testFlag(Qt::ControlModifier) ||
      event->modifiers().testFlag(Qt::AltModifier) ||
      event->modifiers().testFlag(Qt::MetaModifier))
    return false;
  const int steps = event->angleDelta().y() / 120;
  if (steps == 0)
    return false;
  int nearest = 0;
  qreal distance = std::abs(widthOptions_.at(0) - style_.width);
  for (int i = 1; i < widthOptions_.size(); ++i) {
    const qreal candidate = std::abs(widthOptions_.at(i) - style_.width);
    if (candidate < distance) {
      distance = candidate;
      nearest = i;
    }
  }
  const int target = std::clamp(
      nearest + steps, 0, static_cast<int>(widthOptions_.size()) - 1);
  style_.width = widthOptions_.at(target);
  if (repaint_)
    repaint_();
  return true;
}

std::optional<Annotation> EditorCanvas::preview() const {
  IEditorTool *tool = currentTool();
  return tool ? tool->preview() : std::nullopt;
}

void EditorCanvas::resetTool() {
  guides_.clear();
  if (IEditorTool *tool = currentTool())
    tool->reset();
  active_ = false;
}

SnapResult EditorCanvas::snapPoint(const QPointF &point,
                                    const QPointF *anchor,
                                    bool angle) const {
  return snapPointToCanvas(point, anchor, angle, selection_,
                           document_.annotations(), document_.selected());
}

TranslationSnap EditorCanvas::snapTranslation(const QRectF &bounds,
                                               const QPointF &delta) const {
  return snapTranslationToCanvas(bounds, delta, selection_,
                                 document_.annotations(),
                                 document_.selected());
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
  for (const QLineF &guide : guides_) {
    painter.setPen(QPen(QColor(124, 196, 255, 180), 1, Qt::DashLine));
    painter.drawLine(guide);
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
