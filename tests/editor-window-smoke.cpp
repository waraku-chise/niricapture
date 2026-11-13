#include "editor-window-smoke.hpp"

#include "editor-window.hpp"
#include "settings-window.hpp"
#include "editor/font-picker.hpp"

#include <QApplication>
#include <QImage>
#include <QSize>
#include <QTemporaryDir>
#include <QPushButton>
#include <QDir>

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

  QTemporaryDir configRoot;
  if (!configRoot.isValid()) {
    error = QStringLiteral("Could not create settings test directory");
    return false;
  }
  qputenv("XDG_CONFIG_HOME", configRoot.path().toUtf8());
  qputenv("NIRICAPTURE_CONFIG",
          QDir(configRoot.path()).filePath(QStringLiteral("niricapture/config.json")).toUtf8());
  SettingsWindow settings;
  settings.show();
  QApplication::processEvents();
  if (!settings.isVisible()) {
    error = QStringLiteral("SettingsWindow did not become visible");
    return false;
  }
  settings.close();
  QWidget pickerParent;
  pickerParent.resize(800, 600);
  pickerParent.show();
  QApplication::processEvents();
  FontPicker picker(QStringLiteral("Neucha"), &pickerParent,
                    [](const QString &) {});
  picker.showCentered();
  QApplication::processEvents();
  if (!picker.isVisible()) {
    error = QStringLiteral("Embedded FontPicker did not become visible");
    return false;
  }
  if (picker.findChildren<QPushButton *>().size() < 2) {
    error = QStringLiteral("FontPicker is missing its action buttons");
    return false;
  }
  picker.close();
  qunsetenv("NIRICAPTURE_CONFIG");
  qunsetenv("XDG_CONFIG_HOME");
  QApplication::processEvents();
  return true;
}
