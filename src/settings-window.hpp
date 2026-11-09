/** @fileoverview Configuration editor for tools, palette, canvas and theme. */
#pragma once

#include "config.hpp"

#include <QColor>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QLabel;

class SettingsWindow final : public QDialog {
public:
  explicit SettingsWindow(QWidget *parent = nullptr);

private:
  void buildUi();
  void loadConfig();
  void resetUi();
  void saveConfig();
  void openConfigFile();
  void chooseColor(QPushButton *button, QColor &color);
  void refreshColorButton(QPushButton *button, const QColor &color);
  [[nodiscard]] QStringList checkedIds(const QListWidget *list) const;
  void setCheckedIds(QListWidget *list, const QStringList &ids);
  void addColor();
  void removeColor();

  AppConfig config_;
  QTabWidget *tabs_ = nullptr;
  QListWidget *tools_ = nullptr;
  QListWidget *commands_ = nullptr;
  QListWidget *colors_ = nullptr;
  QLineEdit *widths_ = nullptr;
  QSpinBox *canvasWidth_ = nullptr;
  QSpinBox *canvasHeight_ = nullptr;
  QSpinBox *canvasPadding_ = nullptr;
  QComboBox *background_ = nullptr;
  QPushButton *backgroundColor_ = nullptr;
  QPushButton *accent_ = nullptr;
  QPushButton *editorBackground_ = nullptr;
  QPushButton *panel_ = nullptr;
  QPushButton *checkerLight_ = nullptr;
  QPushButton *checkerDark_ = nullptr;
  QPushButton *defaultColor_ = nullptr;
  QSpinBox *defaultWidth_ = nullptr;
  QLineEdit *ocrLanguages_ = nullptr;
  QLineEdit *screenshotDir_ = nullptr;
  QCheckBox *autostart_ = nullptr;
  QLabel *pathLabel_ = nullptr;
  QPushButton *saveButton_ = nullptr;
};
