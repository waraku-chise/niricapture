#include "settings-window.hpp"

#include "capture.hpp"
#include "settings/settings-bridge.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickWidget>
#include <QUrl>
#include <QVBoxLayout>

SettingsWindow::SettingsWindow(QWidget *parent) : QDialog(parent) {
  static_cast<void>(loadCaptureFonts());
  qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
  if (QFontDatabase::hasFamily(QStringLiteral("Roboto"))) {
    QFont font(QStringLiteral("Roboto"));
    font.setPointSizeF(10.5);
    QApplication::setFont(font);
  }

  setWindowTitle(QStringLiteral("设置"));
  resize(920, 700);
  setMinimumSize(820, 600);

  bridge_ = new SettingsBridge(this);
  quick_ = new QQuickWidget(this);
  quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
  quick_->setFocusPolicy(Qt::StrongFocus);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(quick_);

  QQmlContext *context = quick_->engine()->rootContext();
  context->setContextProperty(QStringLiteral("settingsBridge"), bridge_);
  context->setContextProperty(QStringLiteral("initialData"),
                              bridge_->initialData());
  quick_->setSource(QUrl(QStringLiteral("qrc:/settings/qml/Main.qml")));
  for (const QQmlError &error : quick_->errors())
    qWarning().noquote() << error.toString();
  quick_->setFocus();

  connect(bridge_, &SettingsBridge::closeAccepted, this, &QDialog::accept);
}

void SettingsWindow::closeEvent(QCloseEvent *event) {
  if (bridge_ && bridge_->dirty()) {
    bridge_->requestClose();
    event->ignore();
    return;
  }
  QDialog::closeEvent(event);
}
