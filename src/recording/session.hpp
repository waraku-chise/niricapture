/** @fileoverview wf-recorder process/session management for screen recording. */
#pragma once

#include <QObject>
#include <QProcess>
#include <QRect>
#include <QString>

#include <functional>

struct RecordingRequest {
  QRect region;
  QString outputName;
  QString outputPath;
  QString codec = QStringLiteral("libx264");
  int fps = 60;
  bool audio = false;
  QString audioSource;
};

class RecordingSession final : public QObject {
public:
  explicit RecordingSession(RecordingRequest request, QObject *parent = nullptr);
  ~RecordingSession() override;

  bool start(QString *error);
  void stop();
  [[nodiscard]] bool running() const;
  [[nodiscard]] QString outputPath() const { return request_.outputPath; }

  /** Called once after wf-recorder exits; error is empty on success. */
  std::function<void(QString path, QString error)> finished;

private:
  void finishFromProcess();

  RecordingRequest request_;
  QProcess process_;
  bool finishing_ = false;
};

[[nodiscard]] QString recordingSessionPath();
[[nodiscard]] QString recordingSessionDirectory();
[[nodiscard]] QString defaultRecordingDirectory();
[[nodiscard]] bool stopRecording(QString *error);
[[nodiscard]] QString recordingFileName(const QString &directory);
