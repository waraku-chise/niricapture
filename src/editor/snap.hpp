/** @fileoverview Smart snapping to image/annotation edges and 45° angles. */
#pragma once

#include "capture.hpp"

#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QVector>

struct SnapResult {
  QPointF point;
  QVector<QLineF> guides;
};

struct TranslationSnap {
  QPointF delta;
  QVector<QLineF> guides;
};

/**
 * Snaps a point to the nearest x/y guide within `threshold`, then optionally
 * constrains a line endpoint to 0/45/90° around `anchor`. `exclude` skips the
 * annotation currently being edited.
 */
[[nodiscard]] SnapResult snapPointToCanvas(
    const QPointF &point, const QPointF *anchor, bool angleSnap,
    const QRectF &bounds, const QVector<Annotation> &annotations,
    int exclude, qreal threshold = 8.0);

/** Snaps a translated annotation bounds to canvas/annotation guides. */
[[nodiscard]] TranslationSnap snapTranslationToCanvas(
    const QRectF &movingBounds, const QPointF &delta, const QRectF &bounds,
    const QVector<Annotation> &annotations, int exclude,
    qreal threshold = 8.0);
