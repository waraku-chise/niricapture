#include "config-smoke.hpp"

#include "config.hpp"

bool runConfigSmoke(QString &error) {
  const QByteArray json = R"({
    "tools": ["select", "text", "marker"],
    "commands": ["palette", "undo", "copy", "close"],
    "shortcuts": {"text": "Y", "marker": "U"},
    "palette": {"colors": ["#00ff00", "#ff00ff"], "widths": [3, 9]},
    "canvas": {"width": 800, "height": 600, "padding": 24,
                "background": "white"},
    "editor": {"default_color": "#00ff00", "default_width": 3},
    "ocr": {"languages": "eng"},
    "paths": {"screenshots": "/tmp/niricapture-shots"}
  })";
  const AppConfig defaults = AppConfig::defaults();
  QString parseError;
  const AppConfig config = configFromJson(json, defaults, &parseError);
  if (!parseError.isEmpty()) {
    error = parseError;
    return false;
  }
  if (config.tools != QStringList{"select", "text", "marker"} ||
      config.commands !=
          QStringList{"palette", "undo", "copy", "close"}) {
    error = QStringLiteral("Config tool/command lists were not applied");
    return false;
  }
  if (config.shortcuts.value(QStringLiteral("text")) != QStringLiteral("Y") ||
      config.shortcuts.value(QStringLiteral("marker")) != QStringLiteral("U")) {
    error = QStringLiteral("Config shortcuts were not applied");
    return false;
  }
  if (config.palette.colors.size() != 2 ||
      config.palette.widths != QVector<int>{3, 9}) {
    error = QStringLiteral("Config palette was not applied");
    return false;
  }
  if (config.canvas.width != 800 || config.canvas.height != 600 ||
      config.canvas.padding != 24 ||
      config.canvas.backgroundColor != QColor(Qt::white)) {
    error = QStringLiteral("Config canvas was not applied");
    return false;
  }
  if (config.theme.defaultColor != QColor(QStringLiteral("#00ff00")) ||
      config.theme.defaultWidth != 3.0) {
    error = QStringLiteral("Config editor style was not applied");
    return false;
  }
  if (config.ocrLanguages != QStringLiteral("eng") ||
      config.screenshotDir != QStringLiteral("/tmp/niricapture-shots")) {
    error = QStringLiteral("Config OCR/path settings were not applied");
    return false;
  }
  return true;
}
