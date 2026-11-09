#include "config.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {
constexpr const char *kReadme =
    "NiriCapture configuration. tools/commands control order and visibility; "
    "shortcuts are tool ids to Qt key sequences; canvas applies to new images; "
    "colors are #rrggbb or #aarrggbb.";

QString colorValue(const QColor &color) {
  if (!color.isValid())
    return {};
  return color.alpha() < 255 ? color.name(QColor::HexArgb)
                             : color.name(QColor::HexRgb);
}

QColor readColor(const QJsonObject &object, const QString &key,
                 const QColor &fallback) {
  const QString value = object.value(key).toString().trimmed();
  const QColor color(value);
  return color.isValid() ? color : fallback;
}

QJsonObject section(const QJsonObject &root, const QString &name) {
  return root.value(name).toObject();
}

bool migrate(QJsonObject &root, int fromVersion, QString *error) {
  if (fromVersion > AppConfig::kCurrentSchemaVersion) {
    if (error)
      *error = QStringLiteral("Config schema version %1 is newer than supported "
                              "version %2")
                   .arg(fromVersion)
                   .arg(AppConfig::kCurrentSchemaVersion);
    return false;
  }
  int version = std::max(1, fromVersion);
  if (version <= 1) {
    // v2 only makes the schema explicit. Missing fields already inherit
    // defaults, so no structural rewrite is needed.
    version = 2;
  }
  root.insert(QStringLiteral("version"), version);
  return true;
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

QJsonObject AppConfig::toJson() const {
  QJsonObject root = rawDocument_;
  root.insert(QStringLiteral("version"), kCurrentSchemaVersion);
  if (!root.contains(QStringLiteral("_readme")))
    root.insert(QStringLiteral("_readme"), QString::fromLatin1(kReadme));

  root.insert(QStringLiteral("tools"),
              QJsonArray::fromStringList(tools));
  root.insert(QStringLiteral("commands"),
              QJsonArray::fromStringList(commands));

  QJsonObject shortcutObject;
  for (auto it = shortcuts.cbegin(); it != shortcuts.cend(); ++it) {
    if (!it.value().trimmed().isEmpty())
      shortcutObject.insert(it.key(), it.value());
  }
  root.insert(QStringLiteral("shortcuts"), shortcutObject);

  QJsonObject paletteObject = section(root, QStringLiteral("palette"));
  paletteObject.insert(QStringLiteral("colors"),
                       QJsonArray::fromStringList(palette.colors));
  QJsonArray widths;
  for (const int width : palette.widths)
    widths.append(width);
  paletteObject.insert(QStringLiteral("widths"), widths);
  root.insert(QStringLiteral("palette"), paletteObject);

  QJsonObject canvasObject = section(root, QStringLiteral("canvas"));
  canvasObject.insert(QStringLiteral("width"), canvas.width);
  canvasObject.insert(QStringLiteral("height"), canvas.height);
  canvasObject.insert(QStringLiteral("padding"), canvas.padding);
  canvasObject.insert(QStringLiteral("background"), canvas.background);
  canvasObject.insert(QStringLiteral("background_color"),
                      colorValue(canvas.backgroundColor));
  canvasObject.insert(QStringLiteral("fit_on_open"), canvas.fitOnOpen);
  root.insert(QStringLiteral("canvas"), canvasObject);

  QJsonObject editorObject = section(root, QStringLiteral("editor"));
  editorObject.insert(QStringLiteral("accent"), colorValue(theme.accent));
  editorObject.insert(QStringLiteral("background"),
                      colorValue(theme.background));
  editorObject.insert(QStringLiteral("panel"), colorValue(theme.panel));
  editorObject.insert(QStringLiteral("checker_light"),
                      colorValue(theme.checkerLight));
  editorObject.insert(QStringLiteral("checker_dark"),
                      colorValue(theme.checkerDark));
  editorObject.insert(QStringLiteral("default_color"),
                      colorValue(theme.defaultColor));
  editorObject.insert(QStringLiteral("default_width"), theme.defaultWidth);
  root.insert(QStringLiteral("editor"), editorObject);

  QJsonObject ocrObject = section(root, QStringLiteral("ocr"));
  ocrObject.insert(QStringLiteral("languages"), ocrLanguages);
  root.insert(QStringLiteral("ocr"), ocrObject);

  QJsonObject pathsObject = section(root, QStringLiteral("paths"));
  pathsObject.insert(QStringLiteral("screenshots"), screenshotDir);
  root.insert(QStringLiteral("paths"), pathsObject);

  return root;
}

QByteArray AppConfig::toJsonBytes() const {
  return QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
}

bool AppConfig::saveTo(const QString &filePath, QString *error) const {
  if (!writable_) {
    if (error)
      *error = QStringLiteral("Refusing to overwrite an invalid or newer config");
    return false;
  }
  if (filePath.isEmpty()) {
    if (error)
      *error = QStringLiteral("Config path is empty");
    return false;
  }
  const QFileInfo info(filePath);
  if (!QDir().mkpath(info.absolutePath())) {
    if (error)
      *error = QStringLiteral("Could not create config directory %1")
                   .arg(info.absolutePath());
    return false;
  }
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error)
      *error = QStringLiteral("Could not open config %1 for writing: %2")
                   .arg(filePath, file.errorString());
    return false;
  }
  const QByteArray bytes = toJsonBytes();
  if (file.write(bytes) != bytes.size()) {
    if (error)
      *error = QStringLiteral("Could not write config %1: %2")
                   .arg(filePath, file.errorString());
    file.cancelWriting();
    return false;
  }
  if (!file.commit()) {
    if (error)
      *error = QStringLiteral("Could not commit config %1: %2")
                   .arg(filePath, file.errorString());
    return false;
  }
  return true;
}

bool AppConfig::save(QString *error) const { return saveTo(path, error); }

bool AppConfig::writeDefault(const QString &path, QString *error) {
  AppConfig config = defaults();
  config.path = path;
  return config.saveTo(path, error);
}

AppConfig configFromJson(const QByteArray &json, const AppConfig &base,
                         QString *error) {
  AppConfig config = base;
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    if (error)
      *error = QStringLiteral("Config JSON error at offset %1: %2")
                   .arg(parseError.offset)
                   .arg(parseError.errorString());
    AppConfig rejected = base;
    rejected.writable_ = false;
    return rejected;
  }

  QJsonObject root = document.object();
  const int declaredVersion = root.value(QStringLiteral("version")).toInt(1);
  if (!migrate(root, declaredVersion, error)) {
    AppConfig rejected = base;
    rejected.writable_ = false;
    return rejected;
  }
  config.rawDocument_ = root;
  config.schemaVersion_ = AppConfig::kCurrentSchemaVersion;
  config.writable_ = true;

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
    // Keep existing default configurations in sync with the eyedropper added
    // in 1.0 without overriding a genuinely custom tool list.
    const QStringList legacyDefaults{
        QStringLiteral("select"),   QStringLiteral("pen"),
        QStringLiteral("arrow"),    QStringLiteral("line"),
        QStringLiteral("rectangle"), QStringLiteral("ellipse"),
        QStringLiteral("text"),     QStringLiteral("highlighter"),
        QStringLiteral("mosaic"),   QStringLiteral("blur"),
        QStringLiteral("marker")};
    if (config.tools == legacyDefaults)
      config.tools.push_back(QStringLiteral("eyedropper"));
  }
  if (root.contains(QStringLiteral("commands"))) {
    const QStringList commands =
        readStrings(root.value(QStringLiteral("commands")));
    if (!commands.isEmpty())
      config.commands = commands;
  }
  const QJsonObject shortcuts =
      section(root, QStringLiteral("shortcuts"));
  for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it) {
    const QString key = it.value().toString().trimmed();
    if (!key.isEmpty())
      config.shortcuts.insert(it.key(), key);
  }

  const QJsonObject palette = section(root, QStringLiteral("palette"));
  if (palette.contains(QStringLiteral("colors"))) {
    const QStringList colors =
        readStrings(palette.value(QStringLiteral("colors")));
    if (!colors.isEmpty())
      config.palette.colors = colors;
  }
  if (palette.value(QStringLiteral("widths")).isArray()) {
    QVector<int> widths;
    for (const QJsonValue &value :
         palette.value(QStringLiteral("widths")).toArray()) {
      const int width = value.toInt();
      if (width > 0)
        widths.push_back(width);
    }
    if (!widths.isEmpty())
      config.palette.widths = widths;
  }

  const QJsonObject canvas = section(root, QStringLiteral("canvas"));
  config.canvas.width = std::clamp(
      canvas.value(QStringLiteral("width")).toInt(config.canvas.width), 1,
      20000);
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

  const QJsonObject editor = section(root, QStringLiteral("editor"));
  config.theme.accent =
      readColor(editor, QStringLiteral("accent"), config.theme.accent);
  config.theme.background =
      readColor(editor, QStringLiteral("background"), config.theme.background);
  config.theme.panel =
      readColor(editor, QStringLiteral("panel"), config.theme.panel);
  config.theme.checkerLight = readColor(
      editor, QStringLiteral("checker_light"), config.theme.checkerLight);
  config.theme.checkerDark = readColor(
      editor, QStringLiteral("checker_dark"), config.theme.checkerDark);
  config.theme.defaultColor = readColor(
      editor, QStringLiteral("default_color"), config.theme.defaultColor);
  config.theme.defaultWidth = std::clamp(
      editor.value(QStringLiteral("default_width"))
          .toDouble(config.theme.defaultWidth),
      1.0, 96.0);

  const QJsonObject ocr = section(root, QStringLiteral("ocr"));
  config.ocrLanguages =
      ocr.value(QStringLiteral("languages")).toString().trimmed();
  const QJsonObject paths = section(root, QStringLiteral("paths"));
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
