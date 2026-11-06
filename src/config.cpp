#include "config.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>

#include <algorithm>

namespace {
QString colorName(const QColor &color) {
  return color.isValid() ? color.name(QColor::HexRgb) : QString();
}

QColor readColor(const QJsonObject &object, const QString &key,
                 const QColor &fallback) {
  const QString value = object.value(key).toString().trimmed();
  const QColor color(value);
  return color.isValid() ? color : fallback;
}

} // namespace

AppConfig AppConfig::defaults() { return {}; }

QString AppConfig::defaultPath() {
  const QString override = qEnvironmentVariable("NIRICAPTURE_CONFIG").trimmed();
  if (!override.isEmpty())
    return QFileInfo(override).absoluteFilePath();
  QString base =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  if (base.isEmpty())
    base = QDir::homePath() + QStringLiteral("/.config");
  return QDir(base).filePath(QStringLiteral("niricapture/config.json"));
}

bool AppConfig::writeDefault(const QString &path, QString *error) {
  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(
      QStringLiteral("_readme"),
      QStringLiteral("NiriCapture configuration. tools/commands control order "
                     "and visibility; shortcuts are tool ids to Qt key "
                     "sequences; canvas applies to new images; colors are "
                     "#rrggbb."));
  root.insert(QStringLiteral("tools"),
              QJsonArray::fromStringList(AppConfig::defaults().tools));
  root.insert(QStringLiteral("commands"),
              QJsonArray::fromStringList(AppConfig::defaults().commands));
  QJsonObject shortcuts;
  const AppConfig defaults;
  for (auto it = defaults.shortcuts.cbegin(); it != defaults.shortcuts.cend();
       ++it)
    shortcuts.insert(it.key(), it.value());
  root.insert(QStringLiteral("shortcuts"), shortcuts);

  QJsonObject palette;
  palette.insert(QStringLiteral("colors"),
                 QJsonArray::fromStringList(defaults.palette.colors));
  QJsonArray widths;
  for (const int width : defaults.palette.widths)
    widths.append(width);
  palette.insert(QStringLiteral("widths"), widths);
  root.insert(QStringLiteral("palette"), palette);

  QJsonObject canvas;
  canvas.insert(QStringLiteral("width"), defaults.canvas.width);
  canvas.insert(QStringLiteral("height"), defaults.canvas.height);
  canvas.insert(QStringLiteral("padding"), defaults.canvas.padding);
  canvas.insert(QStringLiteral("background"), defaults.canvas.background);
  canvas.insert(QStringLiteral("background_color"), QStringLiteral("#151922"));
  canvas.insert(QStringLiteral("fit_on_open"), defaults.canvas.fitOnOpen);
  root.insert(QStringLiteral("canvas"), canvas);

  QJsonObject editor;
  editor.insert(QStringLiteral("accent"), colorName(defaults.theme.accent));
  editor.insert(QStringLiteral("background"),
                colorName(defaults.theme.background));
  editor.insert(QStringLiteral("panel"), colorName(defaults.theme.panel));
  editor.insert(QStringLiteral("checker_light"),
                colorName(defaults.theme.checkerLight));
  editor.insert(QStringLiteral("checker_dark"),
                colorName(defaults.theme.checkerDark));
  editor.insert(QStringLiteral("default_color"),
                colorName(defaults.theme.defaultColor));
  editor.insert(QStringLiteral("default_width"), defaults.theme.defaultWidth);
  root.insert(QStringLiteral("editor"), editor);

  root.insert(QStringLiteral("ocr"), QJsonObject{
                                         {QStringLiteral("languages"), QString()},
                                     });
  root.insert(QStringLiteral("paths"),
              QJsonObject{{QStringLiteral("screenshots"), QString()}});

  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error)
      *error = QStringLiteral("Could not write config %1: %2")
                   .arg(path, file.errorString());
    return false;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}

AppConfig configFromJson(const QByteArray &json, const AppConfig &base,
                         QString *error) {
  AppConfig config = base;
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    if (error)
      *error = QStringLiteral("Config JSON error: %1")
                   .arg(parseError.errorString());
    return base;
  }
  const QJsonObject root = document.object();

  const auto readStrings = [](const QJsonValue &value) {
    QStringList result;
    for (const QJsonValue &entry : value.toArray()) {
      const QString text = entry.toString().trimmed();
      if (!text.isEmpty())
        result.push_back(text);
    }
    return result;
  };
  if (root.contains(QStringLiteral("tools"))) {
    const QStringList tools = readStrings(root.value(QStringLiteral("tools")));
    if (!tools.isEmpty())
      config.tools = tools;
  }
  if (root.contains(QStringLiteral("commands"))) {
    const QStringList commands =
        readStrings(root.value(QStringLiteral("commands")));
    if (!commands.isEmpty())
      config.commands = commands;
  }
  if (root.value(QStringLiteral("shortcuts")).isObject()) {
    const QJsonObject shortcuts =
        root.value(QStringLiteral("shortcuts")).toObject();
    for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it) {
      const QString key = it.value().toString().trimmed();
      if (!key.isEmpty())
        config.shortcuts.insert(it.key(), key);
    }
  }
  const QJsonObject palette = root.value(QStringLiteral("palette")).toObject();
  if (palette.contains(QStringLiteral("colors"))) {
    const QStringList colors =
        readStrings(palette.value(QStringLiteral("colors")));
    if (!colors.isEmpty())
      config.palette.colors = colors;
  }
  if (palette.value(QStringLiteral("widths")).isArray()) {
    QVector<int> widths;
    for (const QJsonValue &value : palette.value(QStringLiteral("widths")).toArray()) {
      const int width = value.toInt();
      if (width > 0)
        widths.push_back(width);
    }
    if (!widths.isEmpty())
      config.palette.widths = widths;
  }

  const QJsonObject canvas = root.value(QStringLiteral("canvas")).toObject();
  config.canvas.width = std::clamp(
      canvas.value(QStringLiteral("width")).toInt(config.canvas.width), 1, 20000);
  config.canvas.height = std::clamp(
      canvas.value(QStringLiteral("height")).toInt(config.canvas.height), 1,
      20000);
  config.canvas.padding = std::clamp(
      canvas.value(QStringLiteral("padding")).toInt(config.canvas.padding), 0,
      512);
  const QString background =
      canvas.value(QStringLiteral("background")).toString().trimmed().toLower();
  if (!background.isEmpty())
    config.canvas.background = background;
  config.canvas.backgroundColor =
      readColor(canvas, QStringLiteral("background_color"),
                config.canvas.backgroundColor);
  if (!canvas.contains(QStringLiteral("background_color"))) {
    if (background == QStringLiteral("white"))
      config.canvas.backgroundColor = Qt::white;
    else if (background == QStringLiteral("dark"))
      config.canvas.backgroundColor = QColor(QStringLiteral("#151922"));
    else if (background == QStringLiteral("transparent"))
      config.canvas.backgroundColor = Qt::transparent;
  }
  config.canvas.fitOnOpen =
      canvas.value(QStringLiteral("fit_on_open")).toBool(config.canvas.fitOnOpen);

  const QJsonObject editor = root.value(QStringLiteral("editor")).toObject();
  config.theme.accent =
      readColor(editor, QStringLiteral("accent"), config.theme.accent);
  config.theme.background =
      readColor(editor, QStringLiteral("background"), config.theme.background);
  config.theme.panel =
      readColor(editor, QStringLiteral("panel"), config.theme.panel);
  config.theme.checkerLight = readColor(
      editor, QStringLiteral("checker_light"), config.theme.checkerLight);
  config.theme.checkerDark =
      readColor(editor, QStringLiteral("checker_dark"), config.theme.checkerDark);
  config.theme.defaultColor = readColor(
      editor, QStringLiteral("default_color"), config.theme.defaultColor);
  config.theme.defaultWidth = std::clamp(
      editor.value(QStringLiteral("default_width"))
          .toDouble(config.theme.defaultWidth),
      1.0, 96.0);

  const QJsonObject ocr = root.value(QStringLiteral("ocr")).toObject();
  config.ocrLanguages =
      ocr.value(QStringLiteral("languages")).toString().trimmed();
  const QJsonObject paths = root.value(QStringLiteral("paths")).toObject();
  config.screenshotDir =
      paths.value(QStringLiteral("screenshots")).toString().trimmed();
  return config;
}

AppConfig AppConfig::load(QString *error) {
  AppConfig config = defaults();
  config.path = defaultPath();
  if (config.path.isEmpty()) {
    if (error)
      *error = QStringLiteral("Could not determine config path");
    return config;
  }

  QFile file(config.path);
  if (!file.exists()) {
    QString writeError;
    if (!writeDefault(config.path, &writeError)) {
      if (error)
        *error = writeError;
    }
    return config;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    if (error)
      *error = QStringLiteral("Could not read config %1: %2")
                   .arg(config.path, file.errorString());
    return config;
  }
  return configFromJson(file.readAll(), config, error);
}

BackgroundStyle AppConfig::backgroundStyle() const {
  const QString name = canvas.background.trimmed().toLower();
  if (name == QStringLiteral("aurora"))
    return BackgroundStyle::Aurora;
  if (name == QStringLiteral("sunset"))
    return BackgroundStyle::Sunset;
  if (name == QStringLiteral("lagoon"))
    return BackgroundStyle::Lagoon;
  if (name == QStringLiteral("violet"))
    return BackgroundStyle::Violet;
  return BackgroundStyle::None;
}
