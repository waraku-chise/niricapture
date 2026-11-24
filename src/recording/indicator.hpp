/** @fileoverview Layer-shell recording countdown/status pill. */
#pragma once

#include <QRect>
#include "md3.hpp"

#include <QWidget>

class QScreen;

class RecordingIndicator final : public QWidget {
public:
  RecordingIndicator(QScreen *screen, QRect region,
                     const EditorTheme &theme = {});

  void setCountdown(int seconds);
  void setElapsed(qint64 seconds);
  void startRecording();
  void finish();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QScreen *screen_ = nullptr;
  Md3Tokens tokens_;
  QRect region_;
  QString text_;
  bool persistent_ = true;
  bool recording_ = false;
};
