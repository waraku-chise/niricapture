/** @fileoverview Material 3 settings window hosted in a QQuickWidget. */
#pragma once

#include <QDialog>

class QCloseEvent;
class QQuickWidget;
class SettingsBridge;

class SettingsWindow final : public QDialog {
public:
  explicit SettingsWindow(QWidget *parent = nullptr);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  SettingsBridge *bridge_ = nullptr;
  QQuickWidget *quick_ = nullptr;
};
