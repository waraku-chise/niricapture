/** @fileoverview Small layer-shell hint shown while the Niri overview picker is active. */
#pragma once

#include <QWidget>

class WindowPickerHint final : public QWidget {
public:
  explicit WindowPickerHint();

protected:
  void paintEvent(QPaintEvent *event) override;
};
