/** @fileoverview Layer-shell recording countdown/status pill. */
#pragma once

#include <QRect>
#include <QWidget>

class QScreen;

class RecordingIndicator final : public QWidget {
public:
  RecordingIndicator(QScreen *screen, QRect region);

  void setCountdown(int seconds);
  void setElapsed(qint64 seconds);
  void startRecording();
  void finish();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QScreen *screen_ = nullptr;
  QRect region_;
  QString text_;
  bool persistent_ = true;
  bool recording_ = false;
};
