/** @fileoverview Annotation data model with shared undo/redo history. */
#pragma once

#include "capture.hpp"

#include <QVector>

#include <optional>

/** Owns the annotation list, selection and the single undo/redo history. */
class AnnotationDocument {
public:
  struct Snapshot {
    QVector<Annotation> annotations;
    int selected = -1;
    int nextMarker = 1;
    bool operator==(const Snapshot &) const = default;
  };

  [[nodiscard]] const QVector<Annotation> &annotations() const {
    return state_.annotations;
  }
  [[nodiscard]] QVector<Annotation> &annotations() { return state_.annotations; }
  [[nodiscard]] int selected() const { return state_.selected; }
  void setSelected(int index);
  [[nodiscard]] int nextMarker() const { return state_.nextMarker; }
  [[nodiscard]] int consumeMarker() { return state_.nextMarker++; }

  /** Snapshot before a mutation. No-op for identical consecutive snapshots. */
  void pushUndo();
  void undo();
  void redo();
  [[nodiscard]] bool canUndo() const { return !undo_.isEmpty(); }
  [[nodiscard]] bool canRedo() const { return !redo_.isEmpty(); }

  void add(Annotation annotation, bool select = true);
  void removeSelected();
  void clear();

  [[nodiscard]] const Snapshot &state() const { return state_; }
  void restore(const Snapshot &state);

private:
  Snapshot state_;
  QVector<Snapshot> undo_;
  QVector<Snapshot> redo_;
};
