/** @fileoverview Multi-output frozen-frame region selection. */
#pragma once

#include "desktop-capture.hpp"

#include <QObject>
#include <QRectF>
#include <QVector>

#include <cstdint>

class QWidget;
class RegionOverlay;

/**
 * One layer-shell overlay per output, all sharing a single global selection in
 * Niri's logical coordinate space. The desktop is never recaptured while the
 * selection is being adjusted: every overlay paints its own frozen frame.
 */
class RegionSelector final : public QObject {
  Q_OBJECT
public:
  explicit RegionSelector(DesktopFrames frames, QObject *parent = nullptr);
  ~RegionSelector() override;

  /** Maps the overlays and starts accepting input. */
  void start();
  /** Whether a usable selection currently exists. */
  [[nodiscard]] bool hasSelection() const;
  [[nodiscard]] QRectF selection() const { return selection_; }
  [[nodiscard]] qreal selectedScale() const;
  [[nodiscard]] DesktopFrames &frames() { return frames_; }
  [[nodiscard]] const DesktopFrames &frames() const { return frames_; }

signals:
  void accepted(QRectF selection, qreal scale);
  void cancelled();

private:
  friend class RegionOverlay;

  enum class DragMode : std::uint8_t { None, New, Move, Resize };

  void selectionChanged();
  void moveBy(const QPointF &delta);
  void resizeTo(const QString &handle, const QPointF &global);
  void accept();

public:
  /** Cancels the active selection (also used by signal handlers). */
  void cancel();

private:
  void setKeyboardOverlay(RegionOverlay *overlay);
  [[nodiscard]] QPointF clampPoint(const QPointF &point) const;
  [[nodiscard]] QRectF clampSelection(const QRectF &rect) const;

  DesktopFrames frames_;
  QRectF selection_;
  QRectF dragOriginal_;
  QPointF dragStart_;
  QString resizeHandle_;
  QString ownerOutput_;
  QVector<RegionOverlay *> overlays_;
  DragMode dragMode_ = DragMode::None;
  bool started_ = false;
  bool finished_ = false;
};
