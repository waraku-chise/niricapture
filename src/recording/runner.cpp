#include "recording/runner.hpp"

#include "capture.hpp"
#include "i18n.hpp"
#include "recording/indicator.hpp"
#include "recording/region-overlay.hpp"
#include "recording/session.hpp"

#include <QApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

#include <algorithm>

namespace {
QScreen *recordingScreen() {
  QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
  return screen ? screen : QGuiApplication::primaryScreen();
}
} // namespace

int runRecording(const AppConfig &config, RecordingMode mode) {
  if (mode == RecordingMode::None)
    return 2;
  QScreen *screen = recordingScreen();
  if (!screen) {
    qCritical() << ui("No focused screen for recording", "没有可用于录制的屏幕");
    return 1;
  }

  QRect region;
  if (mode == RecordingMode::Region) {
    auto *overlay = new RecordRegionOverlay(screen, config.theme);
    QEventLoop selectionLoop;
    bool selected = false;
    bool cancelled = false;
    overlay->selected = [&](QRect result) {
      region = result;
      selected = true;
      selectionLoop.quit();
    };
    overlay->cancelled = [&] {
      cancelled = true;
      selectionLoop.quit();
    };
    overlay->show();
    selectionLoop.exec();
    if (cancelled || !selected)
      return 0;
  }

  const QString directory = config.recording.directory.trimmed();
  const QString outputDirectory =
      directory.isEmpty() ? defaultRecordingDirectory() : directory;
  RecordingRequest request;
  request.region = region;
  request.outputName = screen->name();
  request.outputPath = recordingFileName(outputDirectory);
  request.codec = config.recording.codec;
  request.fps = config.recording.fps;
  request.audio = config.recording.audio;

  auto *indicator = new RecordingIndicator(screen, region, config.theme);
  const int countdown = std::clamp(config.recording.countdown, 0, 10);
  indicator->show();
  for (int remaining = countdown; remaining > 0; --remaining) {
    indicator->setCountdown(remaining);
    QEventLoop tick;
    QTimer::singleShot(1000, &tick, &QEventLoop::quit);
    tick.exec();
  }

  RecordingSession session(request);
  QEventLoop recordingLoop;
  QString finishError;
  bool finished = false;
  session.finished = [&](QString, QString error) {
    finishError = std::move(error);
    finished = true;
    recordingLoop.quit();
  };
  QString startError;
  if (!session.start(&startError)) {
    qCritical().noquote() << startError;
    indicator->finish();
    return 1;
  }
  indicator->startRecording();

  QElapsedTimer elapsed;
  elapsed.start();
  QTimer update;
  update.setInterval(500);
  QObject::connect(&update, &QTimer::timeout, indicator, [&, indicator] {
    indicator->setElapsed(elapsed.elapsed() / 1000);
  });
  update.start();
  if (!finished)
    recordingLoop.exec();
  update.stop();
  indicator->finish();
  if (!finishError.isEmpty()) {
    qCritical().noquote() << finishError;
    return 1;
  }
  return 0;
}
