#include "recording/session.hpp"

#include "capture.hpp"
#include "i18n.hpp"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include <csignal>
#include <unistd.h>

#include <QProcess>

namespace {
bool isDecimal(const QString &text) {
  if (text.isEmpty())
    return false;
  for (const QChar ch : text) {
    if (!ch.isDigit())
      return false;
  }
  return true;
}

QString ensureRecordingDirectory(const QString &directory, QString *error) {
  const QString path = directory.trimmed().isEmpty()
                           ? defaultRecordingDirectory()
                           : QDir(directory).absolutePath();
  if (path.isEmpty() || !QDir().mkpath(path)) {
    if (error)
      *error = ui("Could not create recording directory: %1",
                  "无法创建录制目录：%1")
                   .arg(path);
    return {};
  }
  return path;
}
} // namespace

QString recordingSessionDirectory() {
  const QString runtime = secureRuntimeDirectory();
  if (runtime.isEmpty())
    return {};
  return QDir(runtime).filePath(QStringLiteral("recording"));
}

QString recordingSessionPath() {
  const QString directory = recordingSessionDirectory();
  return directory.isEmpty()
             ? QString()
             : QDir(directory).filePath(QStringLiteral("session.json"));
}

QString defaultRecordingDirectory() {
  QString movies =
      QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
  if (movies.isEmpty())
    movies = QDir::homePath() + QStringLiteral("/Videos");
  return QDir(movies).filePath(QStringLiteral("NiriCapture"));
}

QString recordingFileName(const QString &directory) {
  const QString stamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
  return QDir(directory).filePath(
      QStringLiteral("NiriCapture_%1.mp4").arg(stamp));
}

namespace {
void notify(const QString &message, const QString &path) {
  QStringList arguments{QStringLiteral("--app-name"), QStringLiteral("niricapture"),
                        QStringLiteral("--expire-time"), QStringLiteral("5000")};
  if (!path.isEmpty())
    arguments << QStringLiteral("--icon") << QStringLiteral("video-x-generic");
  arguments << message;
  QProcess::startDetached(QStringLiteral("notify-send"), arguments);
}

void notifyRecordingStarted(const QString &path) {
  notify(ui("Recording started", "开始录制"), path);
}

void notifyRecordingFinished(const QString &path, const QString &error) {
  if (!error.isEmpty())
    notify(ui("Recording failed: %1", "录制失败：%1").arg(error), {});
  else
    notify(ui("Recording saved: %1", "录制已保存：%1").arg(path), path);
}
} // namespace

RecordingSession::RecordingSession(RecordingRequest request, QObject *parent)
    : QObject(parent), request_(std::move(request)) {
  connect(&process_,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this](int, QProcess::ExitStatus) { finishFromProcess(); });
}

RecordingSession::~RecordingSession() {
  if (process_.state() != QProcess::NotRunning) {
    ::kill(process_.processId(), SIGINT);
    process_.waitForFinished(1500);
  }
  if (!recordingSessionPath().isEmpty())
    QFile::remove(recordingSessionPath());
}

bool RecordingSession::start(QString *error) {
  if (process_.state() != QProcess::NotRunning) {
    if (error)
      *error = ui("A recording is already running", "已有录制正在进行");
    return false;
  }
  const QString executable = QStandardPaths::findExecutable(
      QStringLiteral("wf-recorder"));
  if (executable.isEmpty()) {
    if (error)
      *error = ui("wf-recorder is not installed",
                  "未安装 wf-recorder，请先安装后再录屏");
    return false;
  }
  QString directoryError;
  const QString requestedDirectory =
      request_.outputPath.isEmpty()
          ? QString()
          : QFileInfo(request_.outputPath).absolutePath();
  const QString directory =
      ensureRecordingDirectory(requestedDirectory, &directoryError);
  if (directory.isEmpty()) {
    if (error)
      *error = directoryError;
    return false;
  }
  if (request_.outputPath.isEmpty())
    request_.outputPath = recordingFileName(directory);
  if (!QFileInfo(request_.outputPath).absolutePath().isEmpty())
    QDir().mkpath(QFileInfo(request_.outputPath).absolutePath());

  QStringList arguments{QStringLiteral("-f"), request_.outputPath,
                        QStringLiteral("-r"), QString::number(request_.fps),
                        QStringLiteral("-c"), request_.codec};
  if (request_.region.isValid() && !request_.region.isEmpty()) {
    arguments << QStringLiteral("-g")
              << QStringLiteral("%1,%2 %3x%4")
                     .arg(request_.region.x())
                     .arg(request_.region.y())
                     .arg(request_.region.width())
                     .arg(request_.region.height());
  } else if (!request_.outputName.isEmpty()) {
    arguments << QStringLiteral("-o") << request_.outputName;
  }
  if (request_.audio) {
    if (request_.audioSource.isEmpty())
      arguments << QStringLiteral("--audio");
    else
      arguments << QStringLiteral("--audio=%1").arg(request_.audioSource);
  }

  process_.setProcessChannelMode(QProcess::MergedChannels);
  process_.start(executable, arguments);
  if (!process_.waitForStarted(2000)) {
    if (error)
      *error = ui("Could not start wf-recorder: %1", "无法启动 wf-recorder：%1")
                   .arg(process_.errorString());
    return false;
  }

  const QString session = recordingSessionPath();
  if (!session.isEmpty()) {
    QDir().mkpath(QFileInfo(session).absolutePath());
    QJsonObject object;
    object.insert(QStringLiteral("pid"), process_.processId());
    object.insert(QStringLiteral("app_pid"), QCoreApplication::applicationPid());
    object.insert(QStringLiteral("path"), request_.outputPath);
    object.insert(QStringLiteral("started_at"),
                  QDateTime::currentDateTime().toString(Qt::ISODate));
    object.insert(QStringLiteral("mode"),
                  request_.region.isEmpty() ? QStringLiteral("fullscreen")
                                            : QStringLiteral("region"));
    object.insert(QStringLiteral("backend"), QStringLiteral("wf-recorder"));
    QSaveFile file(session);
    if (file.open(QIODevice::WriteOnly) &&
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) >= 0)
      file.commit();
  }

  qInfo().noquote() << QStringLiteral("Recording to %1").arg(request_.outputPath);
  notifyRecordingStarted(request_.outputPath);
  return true;
}

void RecordingSession::stop() {
  if (process_.state() == QProcess::NotRunning)
    return;
  ::kill(process_.processId(), SIGINT);
}

bool RecordingSession::running() const {
  return process_.state() != QProcess::NotRunning;
}

void RecordingSession::finishFromProcess() {
  if (finishing_)
    return;
  finishing_ = true;
  const QString path = request_.outputPath;
  const bool fileExists = QFileInfo::exists(path) && QFileInfo(path).size() > 0;
  QString detail = QString::fromUtf8(process_.readAll()).trimmed();
  QString error;
  if (!fileExists) {
    error = detail.isEmpty()
                ? ui("Recording produced no video", "录制没有生成视频文件")
                : detail;
  }
  if (!recordingSessionPath().isEmpty())
    QFile::remove(recordingSessionPath());
  notifyRecordingFinished(path, error);
  if (finished)
    finished(path, error);
}

bool stopRecording(QString *error) {
  const QString session = recordingSessionPath();
  QFile file(session);
  if (session.isEmpty() || !file.open(QIODevice::ReadOnly)) {
    if (error)
      *error = ui("No active NiriCapture recording",
                  "当前没有 NiriCapture 录制");
    return false;
  }
  const QJsonObject object =
      QJsonDocument::fromJson(file.readAll()).object();
  const QString pid = QString::number(object.value(QStringLiteral("pid")).toInteger());
  if (!isDecimal(pid)) {
    QFile::remove(session);
    if (error)
      *error = ui("Recording session has no valid process id",
                  "录制会话没有有效的进程 ID");
    return false;
  }
  ::kill(static_cast<pid_t>(pid.toLongLong()), SIGINT);
  for (int i = 0; i < 40 && QFileInfo::exists(session); ++i)
    QThread::msleep(50);
  QFile::remove(session);
  return true;
}
