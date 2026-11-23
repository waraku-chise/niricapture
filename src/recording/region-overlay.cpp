#include "recording/region-overlay.hpp"

#include "cursor-theme.hpp"
#include "i18n.hpp"

#include <LayerShellQt/Window>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWindow>

RecordRegionOverlay::RecordRegionOverlay(QScreen *screen) : screen_(screen) {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setAutoFillBackground(false);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setCursor(themedCursor(QStringLiteral("cross"), Qt::CrossCursor));
  if (screen_)
    setGeometry(screen_->geometry());

  winId();
  QWindow *window = windowHandle();
  LayerShellQt::Window *layer =
      window ? LayerShellQt::Window::get(window) : nullptr;
  if (layer) {
    layer->setScreen(screen_);
    layer->setScope(QStringLiteral("niricapture-record-region"));
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
  QTimer::singleShot(0, this, [this] {
    setFocus(Qt::ActiveWindowFocusReason);
    update();
  });
}

QRect RecordRegionOverlay::selectionRect() const {
  return selection_.normalized().toRect();
}

void RecordRegionOverlay::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QColor dim(8, 10, 14, 145);
  const QRect selected = selectionRect();
  if (selected.isEmpty()) {
    painter.fillRect(rect(), dim);
  } else {
    painter.fillRect(QRect(0, 0, width(), selected.top()), dim);
    painter.fillRect(QRect(0, selected.bottom() + 1, width(),
                           height() - selected.bottom() - 1),
                     dim);
    painter.fillRect(QRect(0, selected.top(), selected.left(),
                           selected.height()),
                     dim);
    painter.fillRect(QRect(selected.right() + 1, selected.top(),
                           width() - selected.right() - 1, selected.height()),
                     dim);
    painter.setPen(QPen(QColor(124, 196, 255), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selected.adjusted(0, 0, -1, -1));
    const QString size = QStringLiteral("%1 × %2")
                             .arg(selected.width())
                             .arg(selected.height());
    const QRect label(selected.left() + 10, selected.top() + 10, 150, 28);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(12, 15, 21, 220));
    painter.drawRoundedRect(label, 5, 5);
    painter.setPen(Qt::white);
    painter.drawText(label, Qt::AlignCenter, size);
  }

  const QString hint =
      selected.isEmpty()
          ? ui("Drag to choose a recording region · Esc to cancel",
               "拖动选择录制区域 · Esc 取消")
          : ui("Release to start recording · Esc to cancel",
               "松开开始录制 · Esc 取消");
  QFont font = painter.font();
  font.setPixelSize(15);
  font.setWeight(QFont::Medium);
  painter.setFont(font);
  const QFontMetricsF metrics(font);
  const qreal hintWidth = metrics.horizontalAdvance(hint) + 36;
  const QRectF hintRect((width() - hintWidth) / 2.0, 26, hintWidth, 42);
  painter.setPen(QPen(QColor(255, 255, 255, 44), 1));
  painter.setBrush(QColor(18, 21, 28, 240));
  painter.drawRoundedRect(hintRect, 10, 10);
  painter.setPen(QColor(238, 242, 248));
  painter.drawText(hintRect, Qt::AlignCenter, hint);
}

void RecordRegionOverlay::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  start_ = event->position();
  selection_ = QRectF(start_, QSizeF(0, 0));
  selecting_ = true;
  update();
}

void RecordRegionOverlay::mouseMoveEvent(QMouseEvent *event) {
  if (!selecting_)
    return;
  selection_ = QRectF(start_, event->position()).normalized();
  update();
}

void RecordRegionOverlay::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton || !selecting_)
    return;
  selecting_ = false;
  selection_ = QRectF(start_, event->position()).normalized();
  if (selection_.width() >= 16 && selection_.height() >= 16)
    finish();
  else
    update();
}

void RecordRegionOverlay::finish() {
  const QRect result = selectionRect();
  hide();
  if (selected)
    selected(result);
  deleteLater();
}

void RecordRegionOverlay::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    hide();
    if (cancelled)
      cancelled();
    deleteLater();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}
