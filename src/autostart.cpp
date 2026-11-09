#include "autostart.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

QString autostartFilePath() {
  QString base =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  if (base.isEmpty())
    base = QDir::homePath() + QStringLiteral("/.config");
  return QDir(base).filePath(QStringLiteral("autostart/niricapture-tray.desktop"));
}

bool isAutostartEnabled() { return QFileInfo::exists(autostartFilePath()); }

bool setAutostartEnabled(bool enabled, QString *error) {
  const QString path = autostartFilePath();
  if (!enabled) {
    if (!QFile::exists(path))
      return true;
    if (!QFile::remove(path)) {
      if (error)
        *error = QStringLiteral("无法删除开机启动文件：%1").arg(path);
      return false;
    }
    return true;
  }

  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    if (error)
      *error = QStringLiteral("无法创建 autostart 目录");
    return false;
  }
  QString executable = QCoreApplication::applicationFilePath();
  executable.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  const QString desktop =
      QStringLiteral("[Desktop Entry]\n"
                     "Type=Application\n"
                     "Name=NiriCapture Tray\n"
                     "Comment=NiriCapture tray menu\n"
                     "Exec=\"%1\" --tray\n"
                     "Terminal=false\n"
                     "NoDisplay=true\n"
                     "X-GNOME-Autostart-enabled=true\n")
          .arg(executable);
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      file.write(desktop.toUtf8()) < 0 || !file.commit()) {
    if (error)
      *error = QStringLiteral("无法写入开机启动文件：%1").arg(path);
    return false;
  }
  return true;
}
