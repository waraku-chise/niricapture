#include "config-smoke.hpp"

#include "config.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

bool runConfigSmoke(QString &error) {
  const QByteArray json = R"({
    "tools": ["select", "text", "marker"],
    "commands": ["palette", "undo", "copy", "close"],
    "shortcuts": {"text": "Y", "marker": "U"},
    "palette": {"colors": ["#00ff00", "#ff00ff"], "widths": [3, 9]},
    "canvas": {"width": 800, "height": 600, "padding": 24,
                "background": "white"},
    "editor": {"default_color": "#00ff00", "default_width": 3,
               "font": "DejaVu Sans"},
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
      config.theme.defaultWidth != 3.0 ||
      config.theme.fontFamily != QStringLiteral("DejaVu Sans")) {
    error = QStringLiteral("Config editor style was not applied");
    return false;
  }
  if (config.ocrLanguages != QStringLiteral("eng") ||
      config.screenshotDir != QStringLiteral("/tmp/niricapture-shots")) {
    error = QStringLiteral("Config OCR/path settings were not applied");
    return false;
  }
  if (config.schemaVersion() != AppConfig::kCurrentSchemaVersion) {
    error = QStringLiteral("Config schema version was not migrated");
    return false;
  }

  const QByteArray preserved = R"({
    "version": 1,
    "future_root": {"enabled": true},
    "canvas": {"width": 320, "future_canvas": "keep"},
    "editor": {"future_editor": 7},
    "palette": {"future_palette": ["keep"]},
    "ocr": {"future_ocr": true},
    "paths": {"future_path": "keep"}
  })";
  const AppConfig preserveConfig =
      configFromJson(preserved, defaults, &parseError);
  if (!parseError.isEmpty()) {
    error = parseError;
    return false;
  }
  const QJsonObject serialized = preserveConfig.toJson();
  if (serialized.value(QStringLiteral("version")).toInt() != 2 ||
      !serialized.value(QStringLiteral("future_root")).isObject() ||
      !serialized.value(QStringLiteral("canvas"))
           .toObject()
           .value(QStringLiteral("future_canvas"))
           .isString() ||
      !serialized.value(QStringLiteral("editor"))
           .toObject()
           .contains(QStringLiteral("future_editor")) ||
      !serialized.value(QStringLiteral("palette"))
           .toObject()
           .contains(QStringLiteral("future_palette")) ||
      !serialized.value(QStringLiteral("ocr"))
           .toObject()
           .contains(QStringLiteral("future_ocr")) ||
      !serialized.value(QStringLiteral("paths"))
           .toObject()
           .contains(QStringLiteral("future_path"))) {
    error = QStringLiteral("Unknown config fields were not preserved");
    return false;
  }

  QString newerError;
  const AppConfig newer = configFromJson(
      R"({"version": 99, "tools": ["text"]})", defaults, &newerError);
  if (newerError.isEmpty() || newer.tools != defaults.tools ||
      newer.writable()) {
    error = QStringLiteral("Newer schema version was not rejected");
    return false;
  }

  QTemporaryDir temporary;
  if (!temporary.isValid()) {
    error = QStringLiteral("Could not create temporary config directory");
    return false;
  }
  AppConfig saved = preserveConfig;
  saved.path = temporary.filePath(QStringLiteral("nested/config.json"));
  QString saveError;
  if (!saved.save(&saveError) || !saveError.isEmpty()) {
    error = saveError.isEmpty() ? QStringLiteral("Config save failed")
                                : saveError;
    return false;
  }
  QFile savedFile(saved.path);
  if (!savedFile.open(QIODevice::ReadOnly)) {
    error = QStringLiteral("Could not reopen saved config");
    return false;
  }
  QString reloadError;
  const AppConfig reloaded =
      configFromJson(savedFile.readAll(), defaults, &reloadError);
  if (!reloadError.isEmpty() ||
      reloaded.palette.colors != preserveConfig.palette.colors ||
      reloaded.canvas.width != preserveConfig.canvas.width ||
      reloaded.theme.defaultColor != preserveConfig.theme.defaultColor) {
    error = reloadError.isEmpty() ? QStringLiteral("Config roundtrip failed")
                                  : reloadError;
    return false;
  }
  return true;
}
