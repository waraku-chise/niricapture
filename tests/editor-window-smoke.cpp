#include "editor-window-smoke.hpp"

#include "editor-window.hpp"

#include <QApplication>
#include <QImage>
#include <QSize>

bool runEditorWindowSmoke(QString &error) {
  EditorWindow window;
  window.resize(1000, 700);
  window.show();
  QApplication::processEvents();

  if (!window.isVisible()) {
    error = QStringLiteral("EditorWindow did not become visible");
    return false;
  }

  window.newCanvas(QSize(400, 300));
  QApplication::processEvents();
  const QImage image = window.grab().toImage();
  if (image.isNull() || image.width() < 100 || image.height() < 100) {
    error = QStringLiteral("EditorWindow canvas did not render");
    return false;
  }
  window.close();
  QApplication::processEvents();
  return true;
}
