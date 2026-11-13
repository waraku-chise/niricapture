#include "settings-bridge.hpp"

#include "md3.hpp"

#include <QApplication>
#include <QColor>
#include <QMetaType>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QDir>
#include <QUrl>

#include <algorithm>

namespace {

QString colorHex(const QColor &color) {
  return color.name(QColor::HexRgb).toUpper();
}

QVariantMap failure(const QString &message) {
  QVariantMap result;
  result.insert(QStringLiteral("ok"), false);
  result.insert(QStringLiteral("error"), message);
  return result;
}

QVariantMap success(const QString &path, const QString &warning) {
  QVariantMap result;
  result.insert(QStringLiteral("ok"), true);
  result.insert(QStringLiteral("path"), path);
  result.insert(QStringLiteral("warning"), warning);
  return result;
}

QVariantList stringList(const QStringList &values) {
  QVariantList result;
  for (const QString &value : values)
    result.append(value);
  return result;
}

QVariantMap serialize(const AppConfig &config) {
  QVariantMap map;
  map.insert(QStringLiteral("tools"), stringList(config.tools));
  map.insert(QStringLiteral("commands"), stringList(config.commands));

  QVariantMap palette;
  palette.insert(QStringLiteral("colors"), stringList(config.palette.colors));
  QVariantList widths;
  for (const int width : config.palette.widths)
    widths.append(width);
  palette.insert(QStringLiteral("widths"), widths);
  map.insert(QStringLiteral("palette"), palette);

  QVariantMap canvas;
  canvas.insert(QStringLiteral("width"), config.canvas.width);
  canvas.insert(QStringLiteral("height"), config.canvas.height);
  canvas.insert(QStringLiteral("padding"), config.canvas.padding);
  canvas.insert(QStringLiteral("background"), config.canvas.background);
  canvas.insert(QStringLiteral("background_color"),
                colorHex(config.canvas.backgroundColor));
  canvas.insert(QStringLiteral("fit_on_open"), config.canvas.fitOnOpen);
  map.insert(QStringLiteral("canvas"), canvas);

  QVariantMap editor;
  editor.insert(QStringLiteral("accent"), colorHex(config.theme.accent));
  editor.insert(QStringLiteral("background"),
                colorHex(config.theme.background));
  editor.insert(QStringLiteral("panel"), colorHex(config.theme.panel));
  editor.insert(QStringLiteral("checker_light"),
                colorHex(config.theme.checkerLight));
  editor.insert(QStringLiteral("checker_dark"),
                colorHex(config.theme.checkerDark));
  editor.insert(QStringLiteral("default_color"),
                colorHex(config.theme.defaultColor));
  editor.insert(QStringLiteral("default_width"), config.theme.defaultWidth);
  editor.insert(QStringLiteral("font"), config.theme.fontFamily);
  map.insert(QStringLiteral("editor"), editor);

  QVariantMap ocr;
  ocr.insert(QStringLiteral("languages"), config.ocrLanguages);
  map.insert(QStringLiteral("ocr"), ocr);

  QVariantMap paths;
  paths.insert(QStringLiteral("screenshots"), config.screenshotDir);
  map.insert(QStringLiteral("paths"), paths);

  map.insert(QStringLiteral("config_path"), config.path);
  return map;
}

bool readColor(const QVariantMap &map, const QString &key, QColor *color,
               QString *error) {
  const QColor parsed(map.value(key).toString().trimmed());
  if (!parsed.isValid()) {
    *error = QStringLiteral("颜色格式无效：%1").arg(key);
    return false;
  }
  *color = parsed;
  return true;
}

QVector<int> parseWidths(const QVariant &value, QString *error) {
  QStringList parts;
  if (value.metaType().id() == QMetaType::QString) {
    parts = value.toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
  } else if (value.canConvert<QVariantList>()) {
    for (const QVariant &entry : value.toList())
      parts.append(entry.toString());
  }
  QVector<int> widths;
  for (const QString &part : parts) {
    bool ok = false;
    const int width = part.trimmed().toInt(&ok);
    if (!ok || width <= 0 || width > 96) {
      *error = QStringLiteral("线宽必须是 1–96 的正整数，用逗号分隔");
      return {};
    }
    widths.append(width);
  }
  return widths;
}

} // namespace

SettingsBridge::SettingsBridge(QObject *parent) : QObject(parent) {
  config_ = AppConfig::load();
}

QVariantMap SettingsBridge::initialData() const {
  return serialize(config_);
}

bool SettingsBridge::writable() const { return config_.writable(); }

QString SettingsBridge::configPath() const { return config_.path; }

QString SettingsBridge::configPathShort() const {
  QString path = config_.path;
  const QString home = QDir::homePath();
  if (path.startsWith(home))
    path = QStringLiteral("~") + path.mid(home.size());
  return path;
}

QString SettingsBridge::version() const {
  const QString version = QCoreApplication::applicationVersion();
  return version.isEmpty() ? QStringLiteral("NiriCapture")
                           : QStringLiteral("NiriCapture %1").arg(version);
}

QVariantList SettingsBridge::toolCatalog() const {
  struct ToolInfo {
    const char *id;
    const char *label;
    const char *shortcut;
  };
  static const ToolInfo tools[] = {
      {"select", "选择 · 移动 / 缩放", "V"}, {"pen", "画笔", "F"},
      {"arrow", "箭头", "A"},               {"line", "直线", "L"},
      {"rectangle", "矩形", "R"},           {"ellipse", "椭圆", "E"},
      {"text", "文字", "T"},                {"highlighter", "高亮", "H"},
      {"mosaic", "马赛克", "D"},            {"blur", "模糊", "B"},
      {"marker", "序号", "C"},              {"eyedropper", "取色器", "I"},
  };
  QVariantList catalog;
  for (const ToolInfo &tool : tools) {
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), QString::fromUtf8(tool.id));
    entry.insert(QStringLiteral("label"), QString::fromUtf8(tool.label));
    entry.insert(QStringLiteral("shortcut"), QString::fromUtf8(tool.shortcut));
    catalog.append(entry);
  }
  return catalog;
}

QVariantList SettingsBridge::commandCatalog() const {
  struct CommandInfo {
    const char *id;
    const char *label;
  };
  static const CommandInfo commands[] = {
      {"palette", "调色板"}, {"font", "文字字体"}, {"undo", "撤销"},
      {"redo", "重做"},      {"ocr", "OCR"},       {"copy", "复制"},
      {"save", "保存"},      {"close", "关闭"},
  };
  QVariantList catalog;
  for (const CommandInfo &command : commands) {
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), QString::fromUtf8(command.id));
    entry.insert(QStringLiteral("label"), QString::fromUtf8(command.label));
    catalog.append(entry);
  }
  return catalog;
}

bool SettingsBridge::dirty() const { return dirty_; }

void SettingsBridge::setDirty(bool dirty) {
  if (dirty_ == dirty)
    return;
  dirty_ = dirty;
  emit dirtyChanged();
}

QVariantMap SettingsBridge::save(const QVariantMap &values) {
  const QStringList tools = values.value(QStringLiteral("tools")).toStringList();
  if (tools.isEmpty())
    return failure(QStringLiteral("至少保留一个工具"));

  const QStringList commands =
      values.value(QStringLiteral("commands")).toStringList();
  if (commands.isEmpty())
    return failure(QStringLiteral("至少保留一个工具栏命令"));

  const QVariantMap palette = values.value(QStringLiteral("palette")).toMap();
  QStringList colors;
  for (const QVariant &entry : palette.value(QStringLiteral("colors")).toList()) {
    const QColor color(entry.toString().trimmed());
    if (!color.isValid())
      return failure(QStringLiteral("调色板包含无效颜色：%1")
                         .arg(entry.toString()));
    colors.append(colorHex(color));
  }
  if (colors.isEmpty())
    return failure(QStringLiteral("调色板至少需要一个颜色"));

  QString widthsError;
  const QVector<int> widths =
      parseWidths(palette.value(QStringLiteral("widths")), &widthsError);
  if (widths.isEmpty())
    return failure(widthsError.isEmpty()
                       ? QStringLiteral("至少保留一个线宽")
                       : widthsError);

  const QVariantMap canvas = values.value(QStringLiteral("canvas")).toMap();
  const QVariantMap editor = values.value(QStringLiteral("editor")).toMap();

  AppConfig next = config_;
  QString colorError;
  if (!readColor(editor, QStringLiteral("accent"), &next.theme.accent,
                 &colorError) ||
      !readColor(editor, QStringLiteral("background"), &next.theme.background,
                 &colorError) ||
      !readColor(editor, QStringLiteral("panel"), &next.theme.panel,
                 &colorError) ||
      !readColor(editor, QStringLiteral("checker_light"),
                 &next.theme.checkerLight, &colorError) ||
      !readColor(editor, QStringLiteral("checker_dark"),
                 &next.theme.checkerDark, &colorError) ||
      !readColor(editor, QStringLiteral("default_color"),
                 &next.theme.defaultColor, &colorError))
    return failure(colorError);

  if (!readColor(canvas, QStringLiteral("background_color"),
                 &next.canvas.backgroundColor, &colorError))
    return failure(colorError);

  next.tools = tools;
  next.commands = commands;
  next.palette.colors = colors;
  next.palette.widths = widths;
  next.canvas.width =
      std::clamp(canvas.value(QStringLiteral("width")).toInt(),
                 1, 20000);
  next.canvas.height =
      std::clamp(canvas.value(QStringLiteral("height")).toInt(),
                 1, 20000);
  next.canvas.padding =
      std::clamp(canvas.value(QStringLiteral("padding")).toInt(), 0, 512);
  const QString background =
      canvas.value(QStringLiteral("background")).toString().toLower();
  const QStringList backgrounds = {
      QStringLiteral("transparent"), QStringLiteral("white"),
      QStringLiteral("dark"),        QStringLiteral("aurora"),
      QStringLiteral("sunset"),      QStringLiteral("lagoon"),
      QStringLiteral("violet")};
  next.canvas.background =
      backgrounds.contains(background) ? background
                                       : QStringLiteral("transparent");
  next.theme.defaultWidth = std::clamp(
      editor.value(QStringLiteral("default_width")).toInt(), 1, 96);
  const QString font =
      editor.value(QStringLiteral("font")).toString().trimmed();
  next.theme.fontFamily = font.isEmpty() ? QStringLiteral("Neucha") : font;

  next.ocrLanguages =
      values.value(QStringLiteral("ocr"))
          .toMap()
          .value(QStringLiteral("languages"))
          .toString()
          .trimmed();
  next.screenshotDir =
      values.value(QStringLiteral("paths"))
          .toMap()
          .value(QStringLiteral("screenshots"))
          .toString()
          .trimmed();

  QString error;
  if (!next.save(&error))
    return failure(error);
  config_ = next;
  return success(config_.path, QString());
}

QVariantMap SettingsBridge::defaults() const {
  return serialize(AppConfig::defaults());
}

void SettingsBridge::chooseDirectory(const QString &current) {
  const QString directory = md3ChooseDirectory(
      QApplication::activeWindow(), QStringLiteral("选择截图目录"), current);
  if (!directory.isEmpty())
    emit directoryChosen(directory);
}

bool SettingsBridge::openConfigFile() {
  return QDesktopServices::openUrl(QUrl::fromLocalFile(config_.path));
}

void SettingsBridge::requestClose() { emit closeRequested(); }

void SettingsBridge::closeWindow() { emit closeAccepted(); }
