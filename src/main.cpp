#include "capture.hpp"
#include "cli-path.hpp"
#include "desktop-capture.hpp"
#include "editor.hpp"
#include "instance-lock.hpp"
#include "pin.hpp"
#include "region-selector.hpp"

#include <LayerShellQt/Window>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QScreen>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include <algorithm>
#include <atomic>
#include <utility>
#include <csignal>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace {
class PosixSignalNotifier final : public QObject {
public:
  explicit PosixSignalNotifier(std::atomic_bool &termination,
                               QObject *parent = nullptr)
      : QObject(parent), termination_(termination) {
    if (::socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
                     fds_) != 0)
      return; // Default signal disposition stays in effect.
    signalFd_ = fds_[0];

    struct sigaction sa{};
    sa.sa_handler = [](int) {
      const int savedErrno = errno;
      const char byte = 1;
      const int fd = signalFd_;
      if (fd >= 0)
        static_cast<void>(::write(fd, &byte, sizeof(byte)));
      errno = savedErrno;
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigintInstalled_ = ::sigaction(SIGINT, &sa, &previousSigint_) == 0;
    sigtermInstalled_ = ::sigaction(SIGTERM, &sa, &previousSigterm_) == 0;
    if (!sigintInstalled_ && !sigtermInstalled_) {
      closeSockets();
      return;
    }

    notifier_ = new QSocketNotifier(fds_[1], QSocketNotifier::Read, this);
    connect(notifier_, &QSocketNotifier::activated, this, [this] {
      notifier_->setEnabled(false);
      char bytes[32];
      while (::read(fds_[1], bytes, sizeof(bytes)) > 0) {
      }
      termination_.store(true, std::memory_order_release);
      QCoreApplication::quit();
    });
  }

  ~PosixSignalNotifier() override {
    if (sigintInstalled_)
      ::sigaction(SIGINT, &previousSigint_, nullptr);
    if (sigtermInstalled_)
      ::sigaction(SIGTERM, &previousSigterm_, nullptr);
    closeSockets();
  }

private:
  void closeSockets() {
    signalFd_ = -1;
    for (int &fd : fds_) {
      if (fd >= 0) {
        ::close(fd);
        fd = -1;
      }
    }
  }

  std::atomic_bool &termination_;
  static inline int fds_[2]{-1, -1};
  static inline volatile sig_atomic_t signalFd_ = -1;
  struct sigaction previousSigint_{};
  struct sigaction previousSigterm_{};
  bool sigintInstalled_ = false;
  bool sigtermInstalled_ = false;
  QSocketNotifier *notifier_ = nullptr;
};

enum class CaptureMode { Region, Window, Fullscreen, All, File };

struct CaptureResult {
  QImage image;
  QRectF logicalRect;
  qreal scale = 1.0;
  QPointF screenOrigin;
  bool anchorToScreen = false;
  QString outputName;
  QString title;
};

struct WindowInfo {
  QString id;
  QString title;
  QString appId;
  int workspaceId = -1;
  bool focused = false;
  bool floating = false;
  QSize size;
};

QString cleanName(const QString &name) {
  return name.isEmpty() ? QStringLiteral("output") : name;
}

QString largestOutputForRect(const DesktopFrames &frames, const QRectF &rect) {
  QString best = frames.outputs.first().monitor.name;
  qreal bestArea = -1.0;
  for (const OutputFrame &frame : frames.outputs) {
    const QRectF intersection =
        rect.intersected(QRectF(frame.monitor.geometry));
    const qreal area = intersection.isEmpty()
                           ? 0.0
                           : intersection.width() * intersection.height();
    if (area > bestArea) {
      bestArea = area;
      best = frame.monitor.name;
    }
  }
  return best;
}

QScreen *screenForOutput(const QString &name) {
  for (QScreen *screen : QGuiApplication::screens()) {
    if (screen->name() == name)
      return screen;
  }
  return QGuiApplication::primaryScreen();
}

bool composeCapture(const DesktopFrames &frames, const QRectF &selection,
                    qreal scale, const QString &outputName,
                    CaptureResult &result, QString &error) {
  QImage image = composeSelection(frames, selection, scale, error);
  if (image.isNull())
    return false;
  result.image = std::move(image);
  result.logicalRect = selection.normalized();
  result.scale = scale;
  result.outputName = outputName;
  return true;
}

bool selectAndComposeRegion(DesktopFrames frames, CaptureResult &result,
                            std::atomic_bool &termination, QString &error) {
  RegionSelector selector(std::move(frames));
  QEventLoop loop;
  bool accepted = false;
  bool cancelled = false;
  QRectF selection;
  qreal scale = 1.0;
  QObject::connect(&selector, &RegionSelector::accepted, &loop,
                   [&](QRectF rect, qreal selectedScale) {
                     selection = rect;
                     scale = selectedScale;
                     accepted = true;
                     loop.quit();
                   });
  QObject::connect(&selector, &RegionSelector::cancelled, &loop, [&] {
    cancelled = true;
    loop.quit();
  });
  QTimer poll;
  QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
    if (termination.load(std::memory_order_acquire)) {
      cancelled = true;
      selector.cancel();
      loop.quit();
    }
  });
  poll.start(100);
  selector.start();
  if (!accepted && !cancelled)
    loop.exec();
  if (!accepted || selection.isEmpty())
    return false;

  // Compose from the selector's original frozen frames. The desktop is never
  // captured again after the overlays are shown.
  const DesktopFrames &frozen = selector.frames();
  const QString outputName = largestOutputForRect(frozen, selection);
  if (!composeCapture(frozen, selection, scale, outputName, result, error))
    return false;
  // Keep the selected image exactly over the pixels it came from. This only
  // applies when the selection belongs to one output; a cross-output capture
  // is fitted and centred because it has no single editor-space origin.
  const OutputFrame *owner = frozen.find(outputName);
  if (owner && QRectF(owner->monitor.geometry).contains(selection)) {
    result.screenOrigin =
        selection.topLeft() - owner->monitor.geometry.topLeft();
    result.anchorToScreen = true;
  }
  return true;
}

bool prepareFileImage(const QString &filePath, bool clipboardInput,
                      CaptureResult &result, QString &error) {
  QImage image;
  QString inputName;
  if (clipboardInput) {
    if (!loadClipboardImage(image, error))
      return false;
    inputName = QStringLiteral("clipboard image");
  } else {
    QString localFile = QUrl(filePath).toLocalFile();
    if (localFile.isEmpty())
      localFile = filePath;
    if (!image.load(localFile)) {
      error = QStringLiteral("Could not load image: %1").arg(filePath);
      return false;
    }
    inputName = std::move(localFile);
  }
  result.image = std::move(image);
  result.logicalRect = QRectF(QPointF(0, 0), QSizeF(result.image.size()));
  result.scale = 1.0;
  result.outputName = inputName;
  qInfo().noquote() << QStringLiteral("Opened %1 for annotation (%2x%3)")
                           .arg(inputName)
                           .arg(result.image.width())
                           .arg(result.image.height());
  return true;
}

QVector<WindowInfo> listWindows(QString &error) {
  QProcess process;
  process.start(QStringLiteral("niri"),
                {QStringLiteral("msg"), QStringLiteral("--json"),
                 QStringLiteral("windows")});
  if (!process.waitForFinished(10000) || process.exitCode() != 0) {
    error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (error.isEmpty())
      error = QStringLiteral("Could not list Niri windows");
    return {};
  }
  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    error = QStringLiteral("Could not parse Niri window list: %1")
                .arg(parseError.errorString());
    return {};
  }
  QVector<WindowInfo> windows;
  const QJsonArray array = document.array();
  for (const QJsonValue value : array) {
    const QJsonObject window = value.toObject();
    const qint64 id =
        static_cast<qint64>(window.value(QStringLiteral("id")).toDouble());
    if (id <= 0)
      continue;
    const QJsonObject layout =
        window.value(QStringLiteral("layout")).toObject();
    const QJsonArray size = layout.value(QStringLiteral("window_size")).toArray();
    windows.push_back(
        {QString::number(id),
         window.value(QStringLiteral("title")).toString(),
         window.value(QStringLiteral("app_id")).toString(),
         window.value(QStringLiteral("workspace_id")).toInt(-1),
         window.value(QStringLiteral("is_focused")).toBool(),
         window.value(QStringLiteral("is_floating")).toBool(),
         QSize(size.size() > 1 ? size.at(0).toInt() : 0,
               size.size() > 1 ? size.at(1).toInt() : 0)});
  }
  return windows;
}

bool pickWindow(WindowTarget &window, std::atomic_bool &termination,
                QString &error) {
  const CaptureOutcome outcome =
      pickWindowTarget(window, error, &termination);
  if (outcome == CaptureOutcome::Cancelled)
    return false;
  return outcome == CaptureOutcome::Success;
}

bool prepareWindowImage(const QString &windowId, CaptureResult &result,
                        std::atomic_bool &termination, QString &error) {
  WindowTarget target;
  if (!windowId.isEmpty()) {
    target.stableId = windowId;
    for (const WindowInfo &window : listWindows(error)) {
      if (window.id == windowId) {
        target.title = window.title.isEmpty() ? window.appId : window.title;
        break;
      }
    }
    if (target.title.isEmpty()) {
      error = QStringLiteral("Niri window %1 does not exist").arg(windowId);
      return false;
    }
  } else if (!pickWindow(target, termination, error)) {
    return false;
  }

  const CaptureOutcome captured = captureWindowSurface(
      target, result.image, error, &termination);
  if (captured == CaptureOutcome::Cancelled)
    return false;
  if (captured == CaptureOutcome::Error)
    return false;
  result.logicalRect = QRectF(QPointF(0, 0), QSizeF(result.image.size()));
  result.scale = 1.0;
  result.outputName = target.stableId;
  result.title = target.title;
  qInfo().noquote()
      << QStringLiteral("Captured Niri window %1 (%2×%3): %4")
             .arg(target.stableId)
             .arg(result.image.width())
             .arg(result.image.height())
             .arg(target.title);
  return true;
}

CaptureData toCaptureData(CaptureResult result) {
  return editorCaptureFromImage(std::move(result.image), result.logicalRect,
                                result.scale, cleanName(result.outputName),
                                result.screenOrigin, result.anchorToScreen);
}

void configureEditorLayer(CaptureEditor &editor, QScreen *screen) {
  editor.setScreen(screen);
  editor.setGeometry(screen->geometry());
  editor.winId();
  QWindow *window = editor.windowHandle();
  LayerShellQt::Window *layer = LayerShellQt::Window::get(window);
  if (!window || !layer) {
    qCritical() << "Could not create capture editor layer";
    return;
  }
  layer->setScope(QStringLiteral("niricapture"));
  layer->setScreen(screen);
  layer->setLayer(LayerShellQt::Window::LayerOverlay);
  LayerShellQt::Window::Anchors anchors;
  anchors.setFlag(LayerShellQt::Window::AnchorTop);
  anchors.setFlag(LayerShellQt::Window::AnchorBottom);
  anchors.setFlag(LayerShellQt::Window::AnchorLeft);
  anchors.setFlag(LayerShellQt::Window::AnchorRight);
  layer->setAnchors(anchors);
  layer->setExclusiveZone(-1);
  layer->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityExclusive);
  layer->setActivateOnShow(true);
}

int openEditor(CaptureData capture, QuickOutputMode quickMode,
               const QString &outputName, std::atomic_bool &termination) {
  if (termination.load(std::memory_order_acquire))
    return 0;
  if (quickMode != QuickOutputMode::None) {
    QString error;
    const QImage image =
        renderCapture(capture, QRectF(QPointF(), capture.previewSize), {},
                      BackgroundStyle::None);
    if (image.isNull() || !quickOutput(image, quickMode, error)) {
      qCritical().noquote() << error;
      sendCaptureNotification(
          QStringLiteral("Screenshot failed: %1").arg(error));
      return 1;
    }
    return 0;
  }

  QScreen *targetScreen = screenForOutput(outputName);
  CaptureEditor editor(std::move(capture), CaptureEditor::CaptureMode::File,
                       QuickOutputMode::None, &termination);
  configureEditorLayer(editor, targetScreen);
  editor.show();
  editor.setFocus(Qt::ActiveWindowFocusReason);
  return QApplication::exec();
}

int failCapture(const QString &error) {
  qCritical().noquote() << error;
  sendCaptureNotification(QStringLiteral("Screenshot failed: %1").arg(error));
  return 1;
}
} // namespace

int main(int argc, char **argv) {
  QCoreApplication::setApplicationName(QStringLiteral("niricapture"));
  QCoreApplication::setApplicationVersion(
      QString::fromLatin1(NIRICAPTURE_VERSION));
  QCoreApplication::setOrganizationName(QStringLiteral("NiriCapture"));
  qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
  // Qt suppresses diagnostics when stderr is not a TTY (common under Niri's
  // keybind spawn and systemd). CLI users still need to see capture errors.
  qputenv("QT_FORCE_STDERR_LOGGING", "1");
  // --help, --version and --list-windows must not need a Wayland socket.
  if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
      qEnvironmentVariableIsEmpty("DISPLAY"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication::setDesktopFileName(QStringLiteral("niricapture"));
  QApplication application(argc, argv);

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral(
      "Native Niri screenshot tool with DMS frozen-frame selection and a\n"
      "non-destructive annotation editor.\n"
      "\n"
      "Region and fullscreen captures freeze every output through the DMS\n"
      "screenshot backend before any overlay is mapped. Window captures use\n"
      "Niri's compositor-side screenshot-window action, so occluded and\n"
      "off-workspace windows are rendered independently of the desktop.\n"
      "\n"
      "Exit codes: 0 success/cancel, 1 capture or lock failure, 2 usage."));
  parser.addHelpOption();
  parser.addVersionOption();
  const QCommandLineOption fullscreenOption(
      QStringLiteral("capture-fullscreen"),
      QStringLiteral("Freeze the focused output and edit the whole screen."));
  const QCommandLineOption allOption(
      QStringLiteral("capture-all"),
      QStringLiteral("Freeze all outputs and edit their combined image."));
  const QCommandLineOption windowOption(
      {QStringLiteral("capture-window"), QStringLiteral("capture-windows")},
      QStringLiteral("Select a window with Niri's native window picker."));
  const QCommandLineOption regionOption(
      QStringLiteral("capture-region"),
      QStringLiteral("Freeze the desktop and select a region (default)."));
  const QCommandLineOption windowIdOption(
      QStringLiteral("window-id"),
      QStringLiteral("Capture this Niri window id without opening the picker."),
      QStringLiteral("id"));
  const QCommandLineOption outputOption(
      QStringLiteral("output"),
      QStringLiteral("Output name for --capture-fullscreen."),
      QStringLiteral("name"));
  const QCommandLineOption listWindowsOption(
      QStringLiteral("list-windows"),
      QStringLiteral("Print Niri's window list as JSON and exit."));
  const QCommandLineOption copyOption(
      QStringLiteral("copy"),
      QStringLiteral("Copy the result directly without opening the editor."));
  const QCommandLineOption saveOption(
      QStringLiteral("save"),
      QStringLiteral("Save the result directly without opening the editor."));
  const QCommandLineOption fileOption(
      QStringLiteral("file"),
      QStringLiteral("Open an existing image file in the annotation editor."),
      QStringLiteral("path"));
  const QCommandLineOption clipboardOption(
      QStringLiteral("clipboard"),
      QStringLiteral("Open the current clipboard image in the editor."));
  const QCommandLineOption pinOption(
      QStringLiteral("pin"),
      QStringLiteral("Show an image as a pinned always-visible layer."),
      QStringLiteral("path"));
  parser.addOption(fullscreenOption);
  parser.addOption(allOption);
  parser.addOption(windowOption);
  parser.addOption(regionOption);
  parser.addOption(windowIdOption);
  parser.addOption(outputOption);
  parser.addOption(listWindowsOption);
  parser.addOption(copyOption);
  parser.addOption(saveOption);
  parser.addOption(fileOption);
  parser.addOption(clipboardOption);
  parser.addOption(pinOption);
  parser.addPositionalArgument(
      QStringLiteral("target"),
      QStringLiteral("Capture mode (region, window, fullscreen, all) or an "
                     "image path."),
      QStringLiteral("[target]"));
  parser.process(application);

  QString filePath = parser.value(fileOption);
  if (parser.isSet(fileOption) && filePath.isEmpty()) {
    qCritical() << "The --file option requires a non-empty path";
    return 2;
  }
  if (parser.isSet(pinOption) && parser.value(pinOption).isEmpty()) {
    qCritical() << "The --pin option requires a non-empty path";
    return 2;
  }
  if (parser.isSet(windowIdOption) &&
      parser.value(windowIdOption).trimmed().isEmpty()) {
    qCritical() << "The --window-id option requires a window id";
    return 2;
  }
  if (parser.isSet(listWindowsOption)) {
    QString error;
    const QVector<WindowInfo> windows = listWindows(error);
    if (!error.isEmpty()) {
      qCritical().noquote() << error;
      return 1;
    }
    QJsonArray array;
    for (const WindowInfo &window : windows) {
      QJsonObject object;
      object.insert(QStringLiteral("id"), window.id);
      object.insert(QStringLiteral("title"), window.title);
      object.insert(QStringLiteral("app_id"), window.appId);
      object.insert(QStringLiteral("workspace_id"), window.workspaceId);
      object.insert(QStringLiteral("is_focused"), window.focused);
      object.insert(QStringLiteral("is_floating"), window.floating);
      object.insert(QStringLiteral("width"), window.size.width());
      object.insert(QStringLiteral("height"), window.size.height());
      array.append(object);
    }
    QTextStream out(stdout);
    out << QJsonDocument(array).toJson(QJsonDocument::Compact) << '\n';
    return 0;
  }

  const bool clipboardInput = parser.isSet(clipboardOption);
  QuickOutputMode quickOutputMode = QuickOutputMode::None;
  if (parser.isSet(copyOption) && parser.isSet(saveOption))
    quickOutputMode = QuickOutputMode::Both;
  else if (parser.isSet(copyOption))
    quickOutputMode = QuickOutputMode::Copy;
  else if (parser.isSet(saveOption))
    quickOutputMode = QuickOutputMode::Save;

  CaptureMode mode = CaptureMode::Region;
  if (parser.isSet(fullscreenOption))
    mode = CaptureMode::Fullscreen;
  else if (parser.isSet(allOption))
    mode = CaptureMode::All;
  else if (parser.isSet(windowOption) || parser.isSet(windowIdOption))
    mode = CaptureMode::Window;
  int requestedModes = parser.isSet(fullscreenOption) +
                       parser.isSet(allOption) + parser.isSet(windowOption) +
                       parser.isSet(regionOption);

  const QStringList positional = parser.positionalArguments();
  if (parser.isSet(pinOption)) {
    if (!filePath.isEmpty() || clipboardInput || requestedModes > 0 ||
        parser.isSet(windowIdOption) || !positional.isEmpty() ||
        quickOutputMode != QuickOutputMode::None) {
      qCritical()
          << "Pinned mode cannot be combined with capture or edit targets";
      return 2;
    }
    QString pinPath = QUrl(parser.value(pinOption)).toLocalFile();
    if (pinPath.isEmpty())
      pinPath = parser.value(pinOption);
    return runPinnedCapture(pinPath);
  }
  if (positional.size() > 1) {
    qCritical() << "Only one capture target may be specified";
    return 2;
  }
  if (!positional.isEmpty()) {
    const QString localTarget = resolveLocalImagePath(positional.first());
    if (filePath.isEmpty() && !localTarget.isEmpty()) {
      filePath = localTarget;
    } else {
      ++requestedModes;
      const QString &target = positional.first();
      if (target == QStringLiteral("fullscreen"))
        mode = CaptureMode::Fullscreen;
      else if (target == QStringLiteral("all"))
        mode = CaptureMode::All;
      else if (target == QStringLiteral("window") ||
               target == QStringLiteral("windows"))
        mode = CaptureMode::Window;
      else if (target == QStringLiteral("region") ||
               target == QStringLiteral("smart"))
        mode = CaptureMode::Region;
      else {
        qCritical().noquote()
            << QStringLiteral("Unknown capture target: %1").arg(target);
        return 2;
      }
    }
  }
  if (filePath.isEmpty() && requestedModes > 1) {
    qCritical() << "Capture mode options are mutually exclusive";
    return 2;
  }
  if (!filePath.isEmpty() && requestedModes > 0) {
    qCritical() << "An image file cannot be combined with a capture mode";
    return 2;
  }
  if (clipboardInput && (!filePath.isEmpty() || requestedModes > 0)) {
    qCritical() << "Clipboard input cannot be combined with another target";
    return 2;
  }
  if (parser.isSet(outputOption) && mode != CaptureMode::Fullscreen) {
    qCritical() << "--output is only valid with --capture-fullscreen";
    return 2;
  }
  if (parser.isSet(windowIdOption) &&
      (parser.isSet(regionOption) || parser.isSet(allOption) ||
       parser.isSet(fullscreenOption))) {
    qCritical() << "--window-id cannot be combined with a screen capture mode";
    return 2;
  }
  if (parser.isSet(windowIdOption) && mode != CaptureMode::Window) {
    qCritical() << "--window-id is only valid for window capture";
    return 2;
  }
  const bool editingImage = clipboardInput || !filePath.isEmpty();
  if (editingImage)
    mode = CaptureMode::File;
  if (editingImage && quickOutputMode != QuickOutputMode::None) {
    qCritical()
        << "Quick output options cannot be combined with an image input";
    return 2;
  }
  if (!loadCaptureFonts())
    return 1;
  application.setQuitOnLastWindowClosed(true);

  const QString runtime = secureRuntimeDirectory();
  if (runtime.isEmpty()) {
    qCritical() << "Could not create private runtime directory";
    return 1;
  }
  QLockFile instanceLock(
      QDir(runtime).filePath(QStringLiteral("niricapture.instance")));
  const InstanceLockResult lockResult = acquireInstanceLock(
      instanceLock, editingImage ? InstanceMode::EditFile
                                 : InstanceMode::Capture);
  if (lockResult.signalledPid != 0)
    qInfo().noquote() << QStringLiteral(
                             "Asked the running niricapture (pid %1) to quit")
                             .arg(lockResult.signalledPid);
  if (!lockResult.proceed) {
    if (!lockResult.error.isEmpty())
      qCritical().noquote() << lockResult.error;
    return lockResult.exitCode;
  }

  std::atomic_bool terminationRequested{false};
  PosixSignalNotifier signalNotifier(terminationRequested, &application);

  QString error;
  CaptureResult result;
  if (editingImage) {
    if (!prepareFileImage(filePath, clipboardInput, result, error))
      return failCapture(error);
    const QString outputName = result.outputName;
    return openEditor(toCaptureData(std::move(result)), quickOutputMode,
                      outputName, terminationRequested);
  }

  if (mode == CaptureMode::Window) {
    if (!prepareWindowImage(parser.value(windowIdOption), result,
                            terminationRequested, error)) {
      if (terminationRequested.load(std::memory_order_acquire))
        return 0;
      if (error.isEmpty())
        return 0;
      return failCapture(error);
    }
    const QString outputName = result.outputName;
    return openEditor(toCaptureData(std::move(result)), quickOutputMode,
                      outputName, terminationRequested);
  }

  if (mode == CaptureMode::Region) {
    DesktopFrames frames;
    if (!captureDesktopFrames(frames, error))
      return failCapture(error);
    if (terminationRequested.load(std::memory_order_acquire))
      return 0;
    if (!selectAndComposeRegion(std::move(frames), result, terminationRequested,
                                error))
      return error.isEmpty() ? 0 : failCapture(error);
    const QString outputName = result.outputName;
    return openEditor(toCaptureData(std::move(result)), quickOutputMode,
                      outputName, terminationRequested);
  }

  DesktopFrames frames;
  if (mode == CaptureMode::All) {
    if (!captureAllOutputFrame(frames, error))
      return failCapture(error);
    const QRectF all = frames.logicalBounds;
    qreal scale = 0.0;
    for (const OutputFrame &frame : frames.outputs)
      scale = std::max(scale, frame.monitor.scale);
    if (scale <= 0.0)
      scale = 1.0;
    if (!composeCapture(frames, all, scale,
                        largestOutputForRect(frames, all), result, error))
      return failCapture(error);
    const QString outputName = result.outputName;
    return openEditor(toCaptureData(std::move(result)), quickOutputMode,
                      outputName, terminationRequested);
  }

  const QString requestedOutput = parser.value(outputOption);
  if (!captureSingleOutputFrame(frames, requestedOutput, error))
    return failCapture(error);
  const OutputFrame &frame = frames.outputs.first();
  if (!composeCapture(frames, QRectF(frame.monitor.geometry),
                      frame.monitor.scale, frame.monitor.name, result, error))
    return failCapture(error);
  // A single-output fullscreen capture has an exact screen origin: keep it at
  // 1:1 under the floating toolbar instead of centring a shrunken copy.
  result.anchorToScreen = true;
  result.screenOrigin = QPointF(0, 0);
  const QString outputName = result.outputName;
  return openEditor(toCaptureData(std::move(result)), quickOutputMode,
                    outputName, terminationRequested);
}
