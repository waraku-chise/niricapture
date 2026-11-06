#include "editor/tools/select-tool.hpp"

#include "editor/annotation-document.hpp"
#include "editor/annotation-geometry.hpp"

#include <QKeyEvent>

#include <algorithm>
#include <QLineF>
#include <QWheelEvent>

void SelectTool::reset() {
  mode_ = Mode::None;
  original_ = {};
  start_ = {};
  index_ = -1;
  handle_ = AnnotationHandle::None;
}

bool SelectTool::mousePress(const ToolEvent &event, ToolContext &context) {
  if (context.document.selected() >= 0 &&
      context.document.selected() < context.document.annotations().size()) {
    const Annotation &selected =
        context.document.annotations().at(context.document.selected());
    const AnnotationHandle handle = annotationHandleAt(selected, event.pos);
    if (handle != AnnotationHandle::None) {
      index_ = context.document.selected();
      original_ = selected;
      handle_ = handle;
      start_ = event.pos;
      mode_ = Mode::Resize;
      context.document.pushUndo();
      context.setStatus(QStringLiteral("Resize"));
      return true;
    }
  }
  const int hit = annotationAt(context.document.annotations(), event.pos);
  if (hit < 0)
    return false;
  context.document.setSelected(hit);
  context.style.color = context.document.annotations().at(hit).color;
  context.style.width = context.document.annotations().at(hit).size;
  context.style.filled = context.document.annotations().at(hit).filled;
  original_ = context.document.annotations().at(hit);
  index_ = hit;
  start_ = event.pos;
  handle_ = AnnotationHandle::None;
  mode_ = Mode::Move;
  context.document.pushUndo();
  return true;
}

bool SelectTool::mouseMove(const ToolEvent &event, ToolContext &context) {
  if (mode_ == Mode::None || index_ < 0 ||
      index_ >= context.document.annotations().size())
    return false;
  Annotation changed = original_;
  if (mode_ == Mode::Move) {
    changed = translatedAnnotation(original_, event.pos - start_);
  } else if (handle_ == AnnotationHandle::Start) {
    changed.start = event.pos;
  } else if (handle_ == AnnotationHandle::End) {
    changed.end = event.pos;
  }
  context.document.annotations()[index_] = std::move(changed);
  context.requestRepaint();
  return true;
}

bool SelectTool::mouseRelease(const ToolEvent &, ToolContext &) {
  const bool consumed = mode_ != Mode::None;
  reset();
  return consumed;
}

bool SelectTool::keyPress(QKeyEvent *event, ToolContext &context) {
  if (event->key() != Qt::Key_Delete && event->key() != Qt::Key_Backspace)
    return false;
  if (context.document.selected() < 0)
    return false;
  context.document.pushUndo();
  context.document.removeSelected();
  context.requestRepaint();
  return true;
}

bool SelectTool::wheel(QWheelEvent *event, ToolContext &context) {
  if (context.document.selected() < 0)
    return false;
  const qreal steps = event->angleDelta().y() / 120.0;
  if (qFuzzyIsNull(steps))
    return false;
  Annotation &annotation =
      context.document.annotations()[context.document.selected()];
  context.document.pushUndo();
  const qreal factor = steps > 0 ? 1.12 : 1.0 / 1.12;
  annotation.size = std::clamp(annotation.size * factor, 2.0, 96.0);
  context.requestRepaint();
  return true;
}
