#include "editor/snap.hpp"

#include "editor/annotation-geometry.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kPi = 3.14159265358979323846;
constexpr qreal kAngleSnapDegrees = 7.0;

QVector<qreal> xCandidates(const QRectF &bounds,
                           const QVector<Annotation> &annotations,
                           int exclude) {
  QVector<qreal> result{bounds.left(), bounds.center().x(), bounds.right()};
  for (int index = 0; index < annotations.size(); ++index) {
    if (index == exclude)
      continue;
    const QRectF box = annotationBounds(annotations.at(index));
    if (box.isEmpty())
      continue;
    result.push_back(box.left());
    result.push_back(box.center().x());
    result.push_back(box.right());
  }
  return result;
}

QVector<qreal> yCandidates(const QRectF &bounds,
                           const QVector<Annotation> &annotations,
                           int exclude) {
  QVector<qreal> result{bounds.top(), bounds.center().y(), bounds.bottom()};
  for (int index = 0; index < annotations.size(); ++index) {
    if (index == exclude)
      continue;
    const QRectF box = annotationBounds(annotations.at(index));
    if (box.isEmpty())
      continue;
    result.push_back(box.top());
    result.push_back(box.center().y());
    result.push_back(box.bottom());
  }
  return result;
}

bool nearest(qreal value, const QVector<qreal> &candidates, qreal threshold,
             qreal &snapped, qreal &guide) {
  bool found = false;
  qreal best = threshold;
  for (const qreal candidate : candidates) {
    const qreal distance = std::abs(candidate - value);
    if (distance <= best) {
      best = distance;
      snapped = candidate;
      guide = candidate;
      found = true;
    }
  }
  return found;
}

void addGuides(const QRectF &bounds, bool snapX, bool snapY, qreal x, qreal y,
               QVector<QLineF> &guides) {
  if (snapX)
    guides.push_back(QLineF(x, bounds.top(), x, bounds.bottom()));
  if (snapY)
    guides.push_back(QLineF(bounds.left(), y, bounds.right(), y));
}
} // namespace

SnapResult snapPointToCanvas(const QPointF &point, const QPointF *anchor,
                             bool angleSnap, const QRectF &bounds,
                             const QVector<Annotation> &annotations,
                             int exclude, qreal threshold) {
  SnapResult result{point, {}};
  if (bounds.isEmpty())
    return result;

  qreal snappedX = point.x();
  qreal snappedY = point.y();
  const bool snapX = nearest(point.x(), xCandidates(bounds, annotations, exclude),
                             threshold, snappedX, snappedX);
  const bool snapY = nearest(point.y(), yCandidates(bounds, annotations, exclude),
                             threshold, snappedY, snappedY);
  result.point = {snappedX, snappedY};
  addGuides(bounds, snapX, snapY, snappedX, snappedY, result.guides);

  if (!angleSnap || !anchor || snapX || snapY)
    return result;

  const QPointF delta = point - *anchor;
  const qreal length = std::hypot(delta.x(), delta.y());
  if (length < 2.0)
    return result;
  const qreal angle = std::atan2(delta.y(), delta.x());
  const qreal step = kPi / 4.0;
  const qreal snappedAngle = std::round(angle / step) * step;
  qreal difference = std::abs(angle - snappedAngle);
  if (difference > kPi)
    difference = 2.0 * kPi - difference;
  if (difference > kAngleSnapDegrees * kPi / 180.0)
    return result;

  result.point =
      *anchor + QPointF(std::cos(snappedAngle), std::sin(snappedAngle)) * length;
  return result;
}

TranslationSnap snapTranslationToCanvas(
    const QRectF &movingBounds, const QPointF &delta, const QRectF &bounds,
    const QVector<Annotation> &annotations, int exclude, qreal threshold) {
  TranslationSnap result{delta, {}};
  if (movingBounds.isEmpty() || bounds.isEmpty())
    return result;

  const QVector<qreal> xs = xCandidates(bounds, annotations, exclude);
  const QVector<qreal> ys = yCandidates(bounds, annotations, exclude);
  const qreal left = movingBounds.left() + delta.x();
  const qreal centerX = movingBounds.center().x() + delta.x();
  const qreal right = movingBounds.right() + delta.x();
  const qreal top = movingBounds.top() + delta.y();
  const qreal centerY = movingBounds.center().y() + delta.y();
  const qreal bottom = movingBounds.bottom() + delta.y();

  qreal bestX = threshold;
  qreal shiftX = 0;
  qreal guideX = 0;
  bool snapX = false;
  for (const qreal candidate : xs) {
    for (const qreal edge : {left, centerX, right}) {
      const qreal diff = candidate - edge;
      if (std::abs(diff) <= bestX) {
        bestX = std::abs(diff);
        shiftX = diff;
        guideX = candidate;
        snapX = true;
      }
    }
  }
  qreal bestY = threshold;
  qreal shiftY = 0;
  qreal guideY = 0;
  bool snapY = false;
  for (const qreal candidate : ys) {
    for (const qreal edge : {top, centerY, bottom}) {
      const qreal diff = candidate - edge;
      if (std::abs(diff) <= bestY) {
        bestY = std::abs(diff);
        shiftY = diff;
        guideY = candidate;
        snapY = true;
      }
    }
  }
  result.delta += QPointF(snapX ? shiftX : 0, snapY ? shiftY : 0);
  addGuides(bounds, snapX, snapY, guideX, guideY, result.guides);
  return result;
}
