/** @fileoverview Frozen desktop frames (optional DMS, grim fallback) and coordinate composition. */
#include "desktop-capture.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace {
struct ProcessResult {
  QByteArray output;
  QByteArray error;
  int exitCode = -1;
  bool finished = false;
};

ProcessResult runCaptureProcess(const QString &program,
                                const QStringList &arguments,
                                qsizetype expectedBytes, int timeoutMs) {
  QProcess process;
  process.setProcessChannelMode(QProcess::SeparateChannels);
  process.start(program, arguments, QIODevice::ReadOnly);
  if (!process.waitForStarted(2000))
    return {{}, process.errorString().toUtf8(), -1, false};

  QByteArray output;
  output.reserve(std::max<qsizetype>(expectedBytes, 0));
  const auto drain = [&process, &output] {
    const qint64 available = process.bytesAvailable();
    if (available <= 0)
      return;
    const qsizetype offset = output.size();
    output.resize(offset + available);
    const qint64 read = process.read(output.data() + offset, available);
    output.resize(offset + std::max<qint64>(read, 0));
  };

  QElapsedTimer clock;
  clock.start();
  const auto remainingMs = [&clock, timeoutMs] {
    return std::max<qint64>(0, timeoutMs - clock.elapsed());
  };
  while (remainingMs() > 0 &&
         process.waitForReadyRead(static_cast<int>(remainingMs())))
    drain();

  const bool finished =
      process.state() == QProcess::NotRunning ||
      process.waitForFinished(static_cast<int>(remainingMs()));
  if (!finished)
    process.kill();
  drain();
  return {std::move(output), process.readAllStandardError(),
          finished ? process.exitCode() : -1, finished};
}

QString trimmedError(const QByteArray &bytes) {
  return QString::fromUtf8(bytes).trimmed();
}

bool parseOutputObject(const QString &name, const QJsonObject &object,
                       MonitorInfo &monitor, QString &error) {
  const QJsonObject logical = object.value(QStringLiteral("logical")).toObject();
  const int width = logical.value(QStringLiteral("width")).toInt();
  const int height = logical.value(QStringLiteral("height")).toInt();
  const qreal scale = logical.value(QStringLiteral("scale")).toDouble();
  if (width <= 0 || height <= 0 || width > 32768 || height > 32768 ||
      scale <= 0.0 || scale > 8.0) {
    error = QStringLiteral("Niri output %1 has invalid logical geometry")
                .arg(name);
    return false;
  }

  monitor = {};
  monitor.name = name;
  monitor.geometry = QRect(logical.value(QStringLiteral("x")).toInt(),
                           logical.value(QStringLiteral("y")).toInt(), width,
                           height);
  monitor.scale = scale;
  // Niri's logical geometry is already transformed; multiplying it by the
  // fractional output scale gives the physical buffer size screenshot tools
  // return, including for rotated outputs.
  monitor.pixelSize = QSize(qRound(width * scale), qRound(height * scale));
  if (monitor.pixelSize.isEmpty()) {
    error = QStringLiteral("Niri output %1 has an empty pixel size").arg(name);
    return false;
  }
  return true;
}
} // namespace

const OutputFrame *DesktopFrames::find(const QString &name) const {
  for (const OutputFrame &frame : outputs) {
    if (frame.monitor.name == name)
      return &frame;
  }
  return nullptr;
}

bool probeOutputs(QVector<MonitorInfo> &outputs, QString &error) {
  outputs.clear();
  error.clear();
  const ProcessResult result =
      runCaptureProcess(QStringLiteral("niri"),
                        {QStringLiteral("msg"), QStringLiteral("--json"),
                         QStringLiteral("outputs")},
                        65536, 10000);
  if (!result.finished || result.exitCode != 0) {
    error = trimmedError(result.error);
    if (error.isEmpty())
      error = QStringLiteral("Could not query Niri outputs");
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(result.output, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    error = QStringLiteral("Could not parse Niri outputs: %1")
                .arg(parseError.errorString());
    return false;
  }

  const QJsonObject root = document.object();
  for (auto it = root.begin(); it != root.end(); ++it) {
    if (!it.value().isObject())
      continue;
    MonitorInfo monitor;
    QString itemError;
    if (!parseOutputObject(it.key(), it.value().toObject(), monitor, itemError)) {
      error = itemError;
      return false;
    }
    outputs.push_back(std::move(monitor));
  }
  if (outputs.isEmpty()) {
    error = QStringLiteral("Niri reported no connected outputs");
    return false;
  }
  std::ranges::sort(outputs, [](const MonitorInfo &a, const MonitorInfo &b) {
    return a.name < b.name;
  });
  return true;
}

bool probeFocusedOutputName(QString &name, QString &error) {
  const ProcessResult result =
      runCaptureProcess(QStringLiteral("niri"),
                        {QStringLiteral("msg"), QStringLiteral("--json"),
                         QStringLiteral("focused-output")},
                        65536, 10000);
  if (!result.finished || result.exitCode != 0) {
    error = trimmedError(result.error);
    if (error.isEmpty())
      error = QStringLiteral("Niri did not report a focused output");
    return false;
  }
  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(result.output, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    error = QStringLiteral("Could not parse Niri focused output: %1")
                .arg(parseError.errorString());
    return false;
  }
  name = document.object().value(QStringLiteral("name")).toString();
  if (name.isEmpty()) {
    error = QStringLiteral("Niri focused output has no name");
    return false;
  }
  return true;
}

bool captureOutputFrame(const MonitorInfo &monitor, OutputFrame &frame,
                        QString &error, bool allowGrimFallback) {
  error.clear();
  // A PNG of this size is at most a few MiB; reserving it up front avoids
  // reallocations while the pipe is drained.
  const qsizetype expected = std::max<qsizetype>(
      1, qsizetype(monitor.pixelSize.width()) * monitor.pixelSize.height() / 2);

  QString dmsError;
  const QString dms = qEnvironmentVariable(
      "NIRICAPTURE_DMS", QStringLiteral("dms"));
  const ProcessResult viaDms = runCaptureProcess(
      dms,
      {QStringLiteral("screenshot"), QStringLiteral("output"),
       QStringLiteral("-o"), monitor.name, QStringLiteral("--stdout"),
       QStringLiteral("--no-clipboard"), QStringLiteral("--no-file"),
       QStringLiteral("--no-notify"), QStringLiteral("--format"),
       QStringLiteral("png")},
      expected, 20000);
  if (viaDms.finished && viaDms.exitCode == 0 && !viaDms.output.isEmpty()) {
    QImage image = QImage::fromData(viaDms.output, "PNG");
    if (!image.isNull()) {
      frame.monitor = monitor;
      frame.monitor.pixelSize = image.size();
      if (monitor.geometry.width() > 0)
        frame.monitor.scale = qreal(image.width()) / monitor.geometry.width();
      frame.image = std::move(image);
      return true;
    }
    dmsError = QStringLiteral("DMS returned data that is not a PNG");
  } else {
    dmsError = trimmedError(viaDms.error);
    if (dmsError.isEmpty())
      dmsError = QStringLiteral("DMS screenshot exited with code %1")
                     .arg(viaDms.exitCode);
  }

  if (!allowGrimFallback) {
    error = QStringLiteral("DMS capture of %1 failed: %2")
                .arg(monitor.name, dmsError);
    return false;
  }

  const ProcessResult viaGrim =
      runCaptureProcess(QStringLiteral("grim"),
                        {QStringLiteral("-o"), monitor.name,
                         QStringLiteral("-t"), QStringLiteral("png"),
                         QStringLiteral("-")},
                        expected, 20000);
  if (viaGrim.finished && viaGrim.exitCode == 0 && !viaGrim.output.isEmpty()) {
    QImage image = QImage::fromData(viaGrim.output, "PNG");
    if (!image.isNull()) {
      frame.monitor = monitor;
      frame.monitor.pixelSize = image.size();
      if (monitor.geometry.width() > 0)
        frame.monitor.scale = qreal(image.width()) / monitor.geometry.width();
      frame.image = std::move(image);
      return true;
    }
  }
  const QString grimError = trimmedError(viaGrim.error);
  error = QStringLiteral("Could not freeze output %1: DMS: %2; grim: %3")
              .arg(monitor.name, dmsError,
                   grimError.isEmpty()
                       ? QStringLiteral("no PNG returned")
                       : grimError);
  return false;
}

bool captureDesktopFrames(DesktopFrames &frames, QString &error) {
  frames = {};
  QVector<MonitorInfo> monitors;
  if (!probeOutputs(monitors, error))
    return false;
  for (const MonitorInfo &monitor : monitors) {
    OutputFrame frame;
    if (!captureOutputFrame(monitor, frame, error))
      return false;
    frames.outputs.push_back(std::move(frame));
  }
  if (frames.outputs.isEmpty()) {
    error = QStringLiteral("No output frames were captured");
    return false;
  }
  QRectF bounds;
  for (const OutputFrame &frame : frames.outputs) {
    const QRectF geometry(frame.monitor.geometry);
    bounds = bounds.isNull() ? geometry : bounds.united(geometry);
  }
  frames.logicalBounds = bounds;
  return true;
}

bool captureSingleOutputFrame(DesktopFrames &frames, const QString &outputName,
                              QString &error) {
  frames = {};
  QVector<MonitorInfo> monitors;
  if (!probeOutputs(monitors, error))
    return false;
  QString name = outputName;
  if (name.isEmpty() && !probeFocusedOutputName(name, error))
    return false;
  const auto it = std::ranges::find_if(monitors, [&name](const MonitorInfo &m) {
    return m.name == name;
  });
  if (it == monitors.end()) {
    error = QStringLiteral("Niri output %1 is not connected").arg(name);
    return false;
  }
  OutputFrame frame;
  if (!captureOutputFrame(*it, frame, error))
    return false;
  frames.outputs.push_back(std::move(frame));
  frames.logicalBounds = QRectF(it->geometry);
  return true;
}

bool captureAllOutputFrame(DesktopFrames &frames, QString &error) {
  return captureDesktopFrames(frames, error);
}

qreal scaleForSelection(const DesktopFrames &frames, const QRectF &selection) {
  qreal bestScale = 1.0;
  qreal bestArea = -1.0;
  for (const OutputFrame &frame : frames.outputs) {
    const QRectF intersection =
        selection.normalized().intersected(QRectF(frame.monitor.geometry));
    if (intersection.isEmpty())
      continue;
    const qreal area = intersection.width() * intersection.height();
    if (area > bestArea) {
      bestArea = area;
      bestScale = frame.monitor.scale;
    }
  }
  if (bestArea < 0.0) {
    for (const OutputFrame &frame : frames.outputs) {
      if (frame.monitor.scale > bestScale)
        bestScale = frame.monitor.scale;
    }
  }
  return bestScale;
}

QImage composeSelection(const DesktopFrames &frames, const QRectF &selection,
                        qreal targetScale, QString &error) {
  error.clear();
  const QRectF normalized = selection.normalized();
  if (normalized.isEmpty() || targetScale <= 0.0) {
    error = QStringLiteral("The selection is empty");
    return {};
  }
  const QSize outputSize(
      std::max(1, qRound(normalized.width() * targetScale)),
      std::max(1, qRound(normalized.height() * targetScale)));
  QImage output(outputSize, QImage::Format_ARGB32_Premultiplied);
  output.fill(Qt::transparent);
  QPainter painter(&output);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.setRenderHint(QPainter::Antialiasing, true);

  bool painted = false;
  for (const OutputFrame &frame : frames.outputs) {
    const QRectF geometry(frame.monitor.geometry);
    const QRectF intersection = normalized.intersected(geometry);
    if (intersection.isEmpty() || frame.image.isNull())
      continue;
    const qreal outputScaleX =
        frame.image.width() / std::max<qreal>(1.0, geometry.width());
    const qreal outputScaleY =
        frame.image.height() / std::max<qreal>(1.0, geometry.height());
    const QRectF source((intersection.left() - geometry.left()) * outputScaleX,
                        (intersection.top() - geometry.top()) * outputScaleY,
                        intersection.width() * outputScaleX,
                        intersection.height() * outputScaleY);
    const QRectF destination(
        (intersection.left() - normalized.left()) * targetScale,
        (intersection.top() - normalized.top()) * targetScale,
        intersection.width() * targetScale,
        intersection.height() * targetScale);
    painter.drawImage(destination, frame.image, source);
    painted = true;
  }
  painter.end();
  if (!painted) {
    error = QStringLiteral("The selection does not overlap any captured output");
    return {};
  }
  return output;
}

QImage composeAllOutputs(const DesktopFrames &frames, qreal targetScale,
                         QString &error) {
  if (frames.logicalBounds.isEmpty()) {
    error = QStringLiteral("No logical desktop bounds were captured");
    return {};
  }
  return composeSelection(frames, frames.logicalBounds, targetScale, error);
}

CaptureData editorCaptureFromImage(QImage image, const QRectF &logicalRect,
                                   qreal scale, const QString &outputName,
                                   const QPointF &screenOrigin,
                                   bool anchorToScreen) {
  CaptureData capture;
  capture.source = std::move(image);
  capture.screenOrigin = screenOrigin;
  capture.anchorToScreen = anchorToScreen;
  capture.previewSize = QSize(std::max(1, qRound(logicalRect.width())),
                              std::max(1, qRound(logicalRect.height())));
  capture.monitor.name = outputName;
  capture.monitor.geometry =
      QRect(QPoint(0, 0), capture.previewSize);
  capture.monitor.pixelSize = capture.source.size();
  capture.monitor.scale = scale > 0.0 ? scale : 1.0;
  return capture;
}
