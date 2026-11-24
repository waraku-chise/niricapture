#include "window-picker-hint.hpp"

#include "i18n.hpp"
#include "cursor-theme.hpp"
#include "md3.hpp"

#include <LayerShellQt/Window>

#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWindow>

WindowPickerHint::WindowPickerHint(const EditorTheme &theme)
    : tokens_(md3Tokens(theme)) {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowDoesNotAcceptFocus);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setCursor(themedCursor(QStringList{QStringLiteral("default"),
                                     QStringLiteral("left_ptr")},
                         Qt::ArrowCursor));
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
  QColor surface = tokens_.surfaceContainerHigh;
  surface.setAlpha(244);
  painter.setPen(QPen(tokens_.outlineVariant, 1));
  painter.setBrush(surface);
  painter.drawRoundedRect(panel, 16, 16);
  QFont font = painter.font();
  font.setPixelSize(14);
  font.setWeight(QFont::Medium);
  painter.setFont(font);
  painter.setPen(tokens_.onSurface);
  painter.drawText(
      panel, Qt::AlignCenter,
      ui("Click a window to capture · Esc to cancel",
         "点击窗口截图 · Esc 取消"));
}
