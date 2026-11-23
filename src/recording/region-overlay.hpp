/** @fileoverview Layer-shell region selector for starting a screen recording. */
#pragma once

#include <QRect>
#include <QWidget>

#include <functional>

class QScreen;

class RecordRegionOverlay final : public QWidget {
public:
  explicit RecordRegionOverlay(QScreen *screen);

  std::function<void(QRect)> selected;
  std::function<void()> cancelled;

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  [[nodiscard]] QRect selectionRect() const;
  void finish();

  QScreen *screen_ = nullptr;
  QRectF selection_;
  QPointF start_;
  bool selecting_ = false;
};
