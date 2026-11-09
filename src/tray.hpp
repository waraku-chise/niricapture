/** @fileoverview Long-running StatusNotifierItem tray menu. */
#pragma once

#include "config.hpp"

#include <QObject>

class QAction;
class QMenu;
class QSystemTrayIcon;

class TrayApp final : public QObject {
public:
  explicit TrayApp(AppConfig config, QObject *parent = nullptr);
  ~TrayApp() override;

  [[nodiscard]] bool available() const;
  bool show();

private:
  void launch(const QStringList &arguments);
  void openSettings();
  void toggleAutostart();
  void refreshAutostart();

  AppConfig config_;
  QSystemTrayIcon *tray_ = nullptr;
  QMenu *menu_ = nullptr;
  QAction *autostart_ = nullptr;
};
