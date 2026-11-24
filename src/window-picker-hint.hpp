/** @fileoverview Small layer-shell hint shown while the Niri overview picker is active. */
#pragma once

#include "config.hpp"
#include "md3.hpp"

#include <QWidget>

class WindowPickerHint final : public QWidget {
public:
  explicit WindowPickerHint(const EditorTheme &theme = {});

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  Md3Tokens tokens_;
};
