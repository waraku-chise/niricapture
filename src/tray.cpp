#include "tray.hpp"

#include "autostart.hpp"
#include "i18n.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QStyle>
#include <QApplication>
#include <QSystemTrayIcon>

TrayApp::TrayApp(AppConfig config, QObject *parent)
    : QObject(parent), config_(std::move(config)) {
  menu_ = new QMenu;
  auto addAction = [this](const QString &text, auto handler) {
    QAction *action = menu_->addAction(text);
    connect(action, &QAction::triggered, this, handler);
    return action;
  };
  addAction(ui("Region capture", "区域截图"), [this] {
    launch({QStringLiteral("--capture-region")});
  });
  addAction(ui("Window capture", "窗口截图"), [this] {
    launch({QStringLiteral("--capture-window")});
  });
  addAction(ui("Focused output", "当前输出"), [this] {
    launch({QStringLiteral("--capture-fullscreen")});
  });
  menu_->addSeparator();
  addAction(ui("Open editor", "打开编辑器"), [this] {
    launch({QStringLiteral("--editor")});
  });
  addAction(ui("Settings", "设置"), [this] { openSettings(); });
  autostart_ = menu_->addAction(ui("Start at login", "开机启动"));
  autostart_->setCheckable(true);
  connect(autostart_, &QAction::triggered, this, &TrayApp::toggleAutostart);
  menu_->addSeparator();
  addAction(ui("Quit", "退出"), [] { QCoreApplication::quit(); });

  QIcon icon = QIcon::fromTheme(QStringLiteral("accessories-screenshot-tool"));
  if (icon.isNull())
    icon = QIcon::fromTheme(QStringLiteral("camera-photo"));
  if (icon.isNull())
    icon = QApplication::style()->standardIcon(QStyle::SP_DesktopIcon);
  tray_ = new QSystemTrayIcon(icon, this);
  tray_->setToolTip(QStringLiteral("NiriCapture"));
  tray_->setContextMenu(menu_);
  connect(tray_, &QSystemTrayIcon::activated, this,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger)
              menu_->popup(QCursor::pos());
          });
  refreshAutostart();
}

TrayApp::~TrayApp() {
  if (tray_)
    tray_->setContextMenu(nullptr);
  delete menu_; // Not owned by the tray icon.
}

bool TrayApp::available() const { return QSystemTrayIcon::isSystemTrayAvailable(); }

bool TrayApp::show() {
  if (!tray_ || !available())
    return false;
  tray_->show();
  return true;
}

void TrayApp::launch(const QStringList &arguments) {
  if (!QProcess::startDetached(QCoreApplication::applicationFilePath(),
                               arguments)) {
    if (tray_)
      tray_->showMessage(QStringLiteral("NiriCapture"),
                         ui("Could not start niricapture", "无法启动 niricapture"));
  }
}

void TrayApp::openSettings() { launch({QStringLiteral("--settings")}); }

void TrayApp::toggleAutostart() {
  QString error;
  const bool enabled = autostart_ && autostart_->isChecked();
  if (!setAutostartEnabled(enabled, &error)) {
    if (tray_)
      tray_->showMessage(QStringLiteral("NiriCapture"), error);
  }
  refreshAutostart();
}

void TrayApp::refreshAutostart() {
  if (autostart_)
    autostart_->setChecked(isAutostartEnabled());
}
