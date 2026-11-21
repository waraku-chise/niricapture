#include "window-picker-hint.hpp"

#include "i18n.hpp"

#include <LayerShellQt/Window>

#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWindow>

WindowPickerHint::WindowPickerHint() {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowDoesNotAcceptFocus);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  resize(430, 56);

  QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  if (screen)
    setScreen(screen);
  winId();
  QWindow *window = windowHandle();
  LayerShellQt::Window *layer =
      window ? LayerShellQt::Window::get(window) : nullptr;
  if (!layer)
    return;
  layer->setScreen(screen);
  layer->setScope(QStringLiteral("niricapture-window-picker-hint"));
  layer->setLayer(LayerShellQt::Window::LayerOverlay);
  LayerShellQt::Window::Anchors anchors;
  anchors.setFlag(LayerShellQt::Window::AnchorTop);
  layer->setAnchors(anchors);
  layer->setMargins(QMargins(0, 18, 0, 0));
  layer->setExclusiveZone(-1);
  layer->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityNone);
  layer->setActivateOnShow(false);
}

void WindowPickerHint::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF panel = rect().adjusted(1, 1, -1, -1);
  painter.setPen(QPen(QColor(255, 255, 255, 38), 1));
  painter.setBrush(QColor(18, 21, 28, 242));
  painter.drawRoundedRect(panel, 12, 12);
  QFont font = painter.font();
  font.setPixelSize(14);
  font.setWeight(QFont::Medium);
  painter.setFont(font);
  painter.setPen(QColor(238, 242, 248));
  painter.drawText(
      panel, Qt::AlignCenter,
      ui("Click a window to capture · Esc to cancel",
         "点击窗口截图 · Esc 取消"));
}
