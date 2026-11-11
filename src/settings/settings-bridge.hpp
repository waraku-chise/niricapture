/** @fileoverview QML bridge for the Material 3 settings window. */
#pragma once

#include "config.hpp"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class SettingsBridge final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool writable READ writable CONSTANT)
  Q_PROPERTY(bool dirty READ dirty WRITE setDirty NOTIFY dirtyChanged)
  Q_PROPERTY(QString configPath READ configPath CONSTANT)
  Q_PROPERTY(QString configPathShort READ configPathShort CONSTANT)
  Q_PROPERTY(QString version READ version CONSTANT)
  Q_PROPERTY(QVariantList toolCatalog READ toolCatalog CONSTANT)
  Q_PROPERTY(QVariantList commandCatalog READ commandCatalog CONSTANT)

public:
  explicit SettingsBridge(QObject *parent = nullptr);

  [[nodiscard]] QVariantMap initialData() const;
  [[nodiscard]] bool writable() const;
  [[nodiscard]] QString configPath() const;
  [[nodiscard]] QString configPathShort() const;
  [[nodiscard]] QString version() const;
  [[nodiscard]] QVariantList toolCatalog() const;
  [[nodiscard]] QVariantList commandCatalog() const;
  [[nodiscard]] bool dirty() const;
  void setDirty(bool dirty);

  Q_INVOKABLE QVariantMap save(const QVariantMap &values);
  Q_INVOKABLE QVariantMap defaults() const;
  Q_INVOKABLE bool openConfigFile();
  Q_INVOKABLE void chooseDirectory(const QString &current);
  Q_INVOKABLE void requestClose();
  Q_INVOKABLE void closeWindow();

signals:
  void dirtyChanged();
  void closeRequested();
  void closeAccepted();
  void directoryChosen(const QString &path);

private:
  AppConfig config_;
  bool dirty_ = false;
};
