/** @fileoverview Shared hit-testing and bounds for annotation data. */
#pragma once

#include "capture.hpp"

#include <QPointF>
#include <QRectF>
#include <QVector>

enum class AnnotationHandle { None, Start, End };

[[nodiscard]] QRectF annotationBounds(const Annotation &annotation);
/** Topmost annotation containing or near `point`; -1 when none. */
[[nodiscard]] int annotationAt(const QVector<Annotation> &annotations,
                               const QPointF &point);
/** Endpoint handle for two-point annotations, when selected. */
[[nodiscard]] AnnotationHandle annotationHandleAt(const Annotation &annotation,
                                                  const QPointF &point,
                                                  qreal tolerance = 16.0);
[[nodiscard]] Annotation translatedAnnotation(const Annotation &annotation,
                                              const QPointF &delta);
[[nodiscard]] bool annotationSupportsEndpoints(const Annotation &annotation);
