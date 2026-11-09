/** @fileoverview In-surface font picker (no native/portal dialog). */
#pragma once

#include <QFrame>

#include <functional>

class QFontComboBox;
class QLabel;

class FontPicker final : public QFrame {
public:
  FontPicker(const QString &family, QWidget *parent,
             std::function<void(const QString &)> accepted);
  void showCentered();

private:
  void finish(const QString &family);
  void updatePreview();
  void cancel();

  QFontComboBox *fonts_ = nullptr;
  QLabel *preview_ = nullptr;
  std::function<void(const QString &)> accepted_;
};
