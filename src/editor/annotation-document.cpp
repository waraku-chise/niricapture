#include "annotation-document.hpp"

#include <algorithm>

void AnnotationDocument::setSelected(int index) {
  state_.selected =
      index >= 0 && index < state_.annotations.size() ? index : -1;
}

void AnnotationDocument::pushUndo() {
  if (!undo_.isEmpty() && undo_.constLast() == state_)
    return;
  undo_.push_back(state_);
  if (undo_.size() > 100)
    undo_.removeFirst();
  redo_.clear();
}

void AnnotationDocument::undo() {
  if (undo_.isEmpty())
    return;
  redo_.push_back(state_);
  state_ = undo_.takeLast();
}

void AnnotationDocument::redo() {
  if (redo_.isEmpty())
    return;
  undo_.push_back(state_);
  state_ = redo_.takeLast();
}

void AnnotationDocument::add(Annotation annotation, bool select) {
  state_.annotations.push_back(std::move(annotation));
  if (select)
    state_.selected = state_.annotations.size() - 1;
}

void AnnotationDocument::removeSelected() {
  if (state_.selected < 0 || state_.selected >= state_.annotations.size())
    return;
  state_.annotations.removeAt(state_.selected);
  state_.selected = -1;
}

void AnnotationDocument::clear() {
  state_ = {};
  undo_.clear();
  redo_.clear();
}

void AnnotationDocument::restore(const Snapshot &state) {
  state_ = state;
  if (state_.selected >= state_.annotations.size())
    state_.selected = -1;
}
