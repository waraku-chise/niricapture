#include "annotation-geometry.hpp"

#include <QFontMetricsF>
#include <QLineF>

#include <algorithm>
#include <cmath>

namespace {
qreal distanceToSegment(const QPointF &point, const QPointF &a,
                        const QPointF &b) {
  const QPointF ab = b - a;
  const qreal lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
  if (lengthSquared <= 0.0001)
    return QLineF(point, a).length();
  const QPointF ap = point - a;
  const qreal t = std::clamp(
      (ap.x() * ab.x() + ap.y() * ab.y()) / lengthSquared, 0.0, 1.0);
  return QLineF(point, a + ab * t).length();
}
} // namespace

QRectF annotationBounds(const Annotation &annotation) {
  if (annotation.kind == Annotation::Kind::Marker) {
    const qreal size = std::max<qreal>(6.0, annotation.size);
    return QRectF(annotation.start.x() - size, annotation.start.y() - size,
                  size * 2, size * 2);
  }
  if (annotation.kind == Annotation::Kind::Text) {
    const QFontMetricsF metrics(annotationTextFont(annotation.size, annotation.fontFamily));
    const qreal width = metrics.horizontalAdvance(annotation.text);
    return QRectF(annotation.start.x(), annotation.start.y() - metrics.ascent(),
                  std::max<qreal>(4.0, width), metrics.height());
  }
  if (annotation.kind == Annotation::Kind::Freehand ||
      annotation.kind == Annotation::Kind::Highlighter) {
    if (annotation.points.isEmpty())
      return {};
    QRectF bounds(annotation.points.first(), QSizeF(0, 0));
    for (const QPointF &point : annotation.points)
      bounds = bounds.united(QRectF(point, QSizeF(0, 0)));
    const qreal pad = std::max<qreal>(5.0, annotation.size + 4.0);
    return bounds.adjusted(-pad, -pad, pad, pad);
  }
  QRectF result = QRectF(annotation.start, annotation.end).normalized();
  const qreal pad = std::max<qreal>(4.0, annotation.size + 2.0);
  return result.adjusted(-pad, -pad, pad, pad);
}

bool annotationSupportsEndpoints(const Annotation &annotation) {
  return annotation.kind != Annotation::Kind::Text &&
         annotation.kind != Annotation::Kind::Marker &&
         annotation.kind != Annotation::Kind::Freehand &&
         annotation.kind != Annotation::Kind::Highlighter;
}

AnnotationHandle annotationHandleAt(const Annotation &annotation,
                                    const QPointF &point, qreal tolerance) {
  if (!annotationSupportsEndpoints(annotation))
    return AnnotationHandle::None;
  if (QLineF(point, annotation.start).length() <= tolerance)
    return AnnotationHandle::Start;
  if (QLineF(point, annotation.end).length() <= tolerance)
    return AnnotationHandle::End;
  return AnnotationHandle::None;
}

int annotationAt(const QVector<Annotation> &annotations, const QPointF &point) {
  for (int index = annotations.size() - 1; index >= 0; --index) {
    const Annotation &annotation = annotations.at(index);
    if (annotation.kind == Annotation::Kind::Text ||
        annotation.kind == Annotation::Kind::Marker ||
        annotation.kind == Annotation::Kind::Redaction ||
        annotation.kind == Annotation::Kind::Blur ||
        annotation.kind == Annotation::Kind::Spotlight) {
      if (annotationBounds(annotation).contains(point))
        return index;
      continue;
    }
    if (annotation.kind == Annotation::Kind::Freehand ||
        annotation.kind == Annotation::Kind::Highlighter) {
      for (int i = 1; i < annotation.points.size(); ++i) {
        if (distanceToSegment(point, annotation.points.at(i - 1),
                              annotation.points.at(i)) <=
            std::max<qreal>(8.0, annotation.size + 3.0))
          return index;
      }
      continue;
    }
    if (annotation.kind == Annotation::Kind::Line ||
        annotation.kind == Annotation::Kind::Arrow) {
      if (distanceToSegment(point, annotation.start, annotation.end) <=
          std::max<qreal>(8.0, annotation.size + 3.0))
        return index;
      continue;
    }
    if (annotation.kind == Annotation::Kind::Ellipse) {
      const QRectF bounds =
          QRectF(annotation.start, annotation.end).normalized();
      if (bounds.width() < 1 || bounds.height() < 1)
        continue;
      const qreal nx = (point.x() - bounds.center().x()) / (bounds.width() / 2);
      const qreal ny = (point.y() - bounds.center().y()) / (bounds.height() / 2);
      if (annotation.filled) {
        if (nx * nx + ny * ny <= 1.05)
          return index;
      } else {
        const qreal distance = std::abs(std::hypot(nx, ny) - 1.0);
        if (distance <= std::max<qreal>(
                            0.08, (annotation.size + 4.0) /
                                      std::max(bounds.width(), bounds.height())))
          return index;
      }
      continue;
    }
    const QRectF bounds =
        QRectF(annotation.start, annotation.end).normalized();
    if (annotation.filled) {
      if (bounds.contains(point))
        return index;
    } else {
      const qreal tolerance = std::max<qreal>(7.0, annotation.size + 3.0);
      const QRectF outer =
          bounds.adjusted(-tolerance, -tolerance, tolerance, tolerance);
      const QRectF inner =
          bounds.adjusted(tolerance, tolerance, -tolerance, -tolerance);
      if (outer.contains(point) && (inner.isEmpty() || !inner.contains(point)))
        return index;
    }
  }
  return -1;
}

Annotation translatedAnnotation(const Annotation &annotation,
                                const QPointF &delta) {
  Annotation result = annotation;
  result.start += delta;
  result.end += delta;
  for (QPointF &point : result.points)
    point += delta;
  return result;
}
