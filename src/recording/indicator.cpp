#include "recording/indicator.hpp"

#include "i18n.hpp"

#include <LayerShellQt/Window>

#include <QFontMetricsF>
#include <QPainter>
#include <QScreen>
#include <QWindow>

#include <algorithm>

namespace {
constexpr int kIndicatorWidth = 310;
constexpr int kIndicatorHeight = 44;
constexpr int kMargin = 18;

QString clockText(qint64 seconds) {
  return QStringLiteral("%1:%2")
      .arg(seconds / 60, 2, 10, QLatin1Char('0'))
      .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

RecordingIndicator::RecordingIndicator(QScreen *screen, QRect region)
    : screen_(screen), region_(region) {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowDoesNotAcceptFocus |
                 Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  resize(kIndicatorWidth, kIndicatorHeight);

  // Pick a corner outside the selected region. A full-screen recording has no
  // safe corner; its indicator is hidden when encoding starts.
  const QSize screenSize = screen_ ? screen_->geometry().size() : QSize(1920, 1080);
  const int right = std::max(0, screenSize.width() - kIndicatorWidth - kMargin);
  const int bottom = std::max(0, screenSize.height() - kIndicatorHeight - kMargin);
  const QVector<QPoint> corners{{kMargin, kMargin},
                                {right, kMargin},
                                {kMargin, bottom},
                                {right, bottom}};
  int chosen = 1;
  bool found = false;
  if (!region_.isEmpty()) {
    for (int index = 0; index < corners.size(); ++index) {
      const QRect candidate(corners.at(index),
                            QSize(kIndicatorWidth, kIndicatorHeight));
      if (!candidate.intersects(region_.adjusted(-8, -8, 8, 8))) {
        chosen = index;
        found = true;
        break;
      }
    }
  }
  // A full-screen recording has no safe corner: hide the indicator as soon as
  // encoding starts so it cannot end up in the video.
  persistent_ = found;

  LayerShellQt::Window *layer = nullptr;
  if (screen_) {
    setScreen(screen_);
    winId();
    QWindow *window = windowHandle();
    layer = window ? LayerShellQt::Window::get(window) : nullptr;
  }
  if (!layer)
    return;
  layer->setScreen(screen_);
  layer->setScope(QStringLiteral("niricapture-recording"));
  layer->setLayer(LayerShellQt::Window::LayerOverlay);
  LayerShellQt::Window::Anchors anchors;
  QMargins margins;
  switch (chosen) {
  case 0:
    anchors.setFlag(LayerShellQt::Window::AnchorTop);
    anchors.setFlag(LayerShellQt::Window::AnchorLeft);
    margins = QMargins(kMargin, kMargin, 0, 0);
    break;
  case 2:
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorLeft);
    margins = QMargins(kMargin, 0, 0, kMargin);
    break;
  case 3:
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorRight);
    margins = QMargins(0, 0, kMargin, kMargin);
    break;
  default:
    anchors.setFlag(LayerShellQt::Window::AnchorTop);
    anchors.setFlag(LayerShellQt::Window::AnchorRight);
    margins = QMargins(0, kMargin, kMargin, 0);
    break;
  }
  layer->setAnchors(anchors);
  layer->setMargins(margins);
  layer->setExclusiveZone(-1);
  layer->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityNone);
  layer->setActivateOnShow(false);
}

void RecordingIndicator::setCountdown(int seconds) {
  text_ = QStringLiteral("%1  ·  %2")
              .arg(seconds)
              .arg(ui("Starting recording", "准备录制"));
  update();
}

void RecordingIndicator::setElapsed(qint64 seconds) {
  text_ = QStringLiteral("● REC %1  ·  %2")
              .arg(clockText(seconds), ui("Mod+Shift+S stops", "Mod+Shift+S 停止"));
  recording_ = true;
  update();
}

void RecordingIndicator::startRecording() {
  if (!persistent_) {
    hide();
    return;
  }
  text_ = QStringLiteral("● REC 00:00  ·  %1")
              .arg(ui("Mod+Shift+S stops", "Mod+Shift+S 停止"));
  recording_ = true;
  update();
}

void RecordingIndicator::finish() {
  hide();
  deleteLater();
}

void RecordingIndicator::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF panel = rect().adjusted(1, 1, -1, -1);
  painter.setPen(QPen(QColor(255, 255, 255, 38), 1));
  painter.setBrush(QColor(18, 21, 28, 242));
  painter.drawRoundedRect(panel, 12, 12);

  QFont font = painter.font();
  font.setPixelSize(13);
  font.setWeight(QFont::Medium);
  painter.setFont(font);
  painter.setPen(QColor(238, 242, 248));
  painter.drawText(panel.adjusted(14, 0, -12, 0), Qt::AlignVCenter, text_);

  painter.setPen(Qt::NoPen);
  painter.setBrush(recording_ ? QColor(QStringLiteral("#ff3b4f"))
                              : QColor(QStringLiteral("#f2b84b")));
  painter.drawEllipse(QPointF(panel.right() - 18, panel.center().y()), 6, 6);
}
