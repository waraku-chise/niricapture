#include "settings-window.hpp"

#include "editor/tool-registry.hpp"
#include "editor/tools/builtin-tools.hpp"
#include "autostart.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString backgroundKey(int index) {
  switch (index) {
  case 1:
    return QStringLiteral("white");
  case 2:
    return QStringLiteral("dark");
  case 3:
    return QStringLiteral("aurora");
  case 4:
    return QStringLiteral("sunset");
  case 5:
    return QStringLiteral("lagoon");
  case 6:
    return QStringLiteral("violet");
  default:
    return QStringLiteral("transparent");
  }
}

int backgroundIndex(const AppConfig &config) {
  const QString name = config.canvas.background.toLower();
  if (name == QStringLiteral("white"))
    return 1;
  if (name == QStringLiteral("dark"))
    return 2;
  if (name == QStringLiteral("aurora"))
    return 3;
  if (name == QStringLiteral("sunset"))
    return 4;
  if (name == QStringLiteral("lagoon"))
    return 5;
  if (name == QStringLiteral("violet"))
    return 6;
  return 0;
}

QStringList commandIds() {
  return {QStringLiteral("palette"), QStringLiteral("font"),
          QStringLiteral("undo"),    QStringLiteral("redo"),
          QStringLiteral("ocr"),     QStringLiteral("copy"),
          QStringLiteral("save"),    QStringLiteral("close")};
}
} // namespace

SettingsWindow::SettingsWindow(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("设置"));
  resize(900, 720);
  setMinimumSize(760, 600);
  config_ = AppConfig::load();
  buildUi();
  loadConfig();
}

void SettingsWindow::buildUi() {
  setStyleSheet(QStringLiteral(R"(
    QDialog { background: #0f1116; color: #eef1f7; }
    QTabWidget::pane { border: 1px solid #262c38; background: #12161e; }
    QTabBar::tab { background: #171c25; color: #aeb8c7; padding: 8px 16px;
                   border: 1px solid #262c38; border-bottom: none; }
    QTabBar::tab:selected { background: #202633; color: #ffffff; }
    QLabel { background: transparent; }
    QLineEdit, QSpinBox, QComboBox { background: #171c25; color: #eef1f7;
      border: 1px solid #303849; border-radius: 6px; padding: 5px 7px; }
    QListWidget { background: #12161e; color: #eef1f7; border: 1px solid #303849;
                  border-radius: 6px; }
    QPushButton { background: #202633; color: #eef1f7; border: 1px solid #303849;
                  border-radius: 7px; padding: 6px 12px; }
    QPushButton:hover { background: #2a3343; }
    QGroupBox { border: 1px solid #262c38; border-radius: 8px; margin-top: 12px;
                padding-top: 10px; }
    QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px;
                       color: #9aa3b2; }
  )"));

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 14);
  tabs_ = new QTabWidget(this);
  layout->addWidget(tabs_, 1);

  // Tools.
  auto *toolsPage = new QWidget(tabs_);
  auto *toolsLayout = new QHBoxLayout(toolsPage);
  tools_ = new QListWidget(toolsPage);
  tools_->setSelectionMode(QAbstractItemView::SingleSelection);
  ToolRegistry registry;
  registerBuiltinTools(registry);
  for (const auto &tool : registry.tools()) {
    auto *item = new QListWidgetItem(tool->id() + QStringLiteral("  ·  ") +
                                        tool->tooltip(),
                                    tools_);
    item->setData(Qt::UserRole, tool->id());
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);
  }
  toolsLayout->addWidget(tools_, 1);
  auto *moveColumn = new QVBoxLayout;
  auto *up = new QPushButton(QStringLiteral("↑"), toolsPage);
  auto *down = new QPushButton(QStringLiteral("↓"), toolsPage);
  moveColumn->addWidget(up);
  moveColumn->addWidget(down);
  moveColumn->addStretch(1);
  toolsLayout->addLayout(moveColumn);
  auto moveItem = [this](int delta) {
    const int row = tools_->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= tools_->count())
      return;
    QListWidgetItem *item = tools_->takeItem(row);
    tools_->insertItem(target, item);
    tools_->setCurrentRow(target);
  };
  connect(up, &QPushButton::clicked, this, [moveItem] { moveItem(-1); });
  connect(down, &QPushButton::clicked, this, [moveItem] { moveItem(1); });
  tabs_->addTab(toolsPage, QStringLiteral("工具栏"));

  // Commands.
  auto *commandsPage = new QWidget(tabs_);
  auto *commandsLayout = new QHBoxLayout(commandsPage);
  commands_ = new QListWidget(commandsPage);
  for (const QString &id : commandIds()) {
    auto *item = new QListWidgetItem(id, commands_);
    item->setData(Qt::UserRole, id);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);
  }
  commandsLayout->addWidget(commands_, 1);
  tabs_->addTab(commandsPage, QStringLiteral("命令"));

  // Palette.
  auto *palettePage = new QWidget(tabs_);
  auto *paletteLayout = new QVBoxLayout(palettePage);
  colors_ = new QListWidget(palettePage);
  paletteLayout->addWidget(colors_, 1);
  auto *paletteButtons = new QHBoxLayout;
  auto *add = new QPushButton(QStringLiteral("添加颜色"), palettePage);
  auto *remove = new QPushButton(QStringLiteral("删除颜色"), palettePage);
  paletteButtons->addWidget(add);
  paletteButtons->addWidget(remove);
  paletteButtons->addStretch(1);
  paletteLayout->addLayout(paletteButtons);
  auto *widthLabel = new QLabel(QStringLiteral("线宽（逗号分隔）"), palettePage);
  widths_ = new QLineEdit(palettePage);
  paletteLayout->addWidget(widthLabel);
  paletteLayout->addWidget(widths_);
  connect(add, &QPushButton::clicked, this, &SettingsWindow::addColor);
  connect(remove, &QPushButton::clicked, this, &SettingsWindow::removeColor);
  tabs_->addTab(palettePage, QStringLiteral("调色板"));

  // Canvas.
  auto *canvasPage = new QWidget(tabs_);
  auto *canvasForm = new QFormLayout(canvasPage);
  canvasWidth_ = new QSpinBox(canvasPage);
  canvasWidth_->setRange(1, 20000);
  canvasHeight_ = new QSpinBox(canvasPage);
  canvasHeight_->setRange(1, 20000);
  canvasPadding_ = new QSpinBox(canvasPage);
  canvasPadding_->setRange(0, 512);
  background_ = new QComboBox(canvasPage);
  background_->addItems({QStringLiteral("透明"), QStringLiteral("白色"),
                         QStringLiteral("深色"), QStringLiteral("Aurora"),
                         QStringLiteral("Sunset"), QStringLiteral("Lagoon"),
                         QStringLiteral("Violet")});
  backgroundColor_ = new QPushButton(canvasPage);
  backgroundColor_->setMinimumWidth(120);
  canvasForm->addRow(QStringLiteral("默认宽度"), canvasWidth_);
  canvasForm->addRow(QStringLiteral("默认高度"), canvasHeight_);
  canvasForm->addRow(QStringLiteral("内边距"), canvasPadding_);
  canvasForm->addRow(QStringLiteral("背景"), background_);
  canvasForm->addRow(QStringLiteral("背景色"), backgroundColor_);
  connect(backgroundColor_, &QPushButton::clicked, this,
          [this] { chooseColor(backgroundColor_, config_.canvas.backgroundColor); });
  tabs_->addTab(canvasPage, QStringLiteral("画布"));

  // Theme.
  auto *themePage = new QWidget(tabs_);
  auto *themeForm = new QFormLayout(themePage);
  accent_ = new QPushButton(themePage);
  editorBackground_ = new QPushButton(themePage);
  panel_ = new QPushButton(themePage);
  checkerLight_ = new QPushButton(themePage);
  checkerDark_ = new QPushButton(themePage);
  defaultColor_ = new QPushButton(themePage);
  defaultWidth_ = new QSpinBox(themePage);
  defaultWidth_->setRange(1, 96);
  font_ = new QFontComboBox(themePage);
  for (QPushButton *button : {accent_, editorBackground_, panel_, checkerLight_,
                              checkerDark_, defaultColor_})
    button->setMinimumWidth(120);
  themeForm->addRow(QStringLiteral("强调色"), accent_);
  themeForm->addRow(QStringLiteral("编辑器背景"), editorBackground_);
  themeForm->addRow(QStringLiteral("面板"), panel_);
  themeForm->addRow(QStringLiteral("棋盘格亮色"), checkerLight_);
  themeForm->addRow(QStringLiteral("棋盘格暗色"), checkerDark_);
  themeForm->addRow(QStringLiteral("默认标注颜色"), defaultColor_);
  themeForm->addRow(QStringLiteral("默认线宽"), defaultWidth_);
  themeForm->addRow(QStringLiteral("文字字体"), font_);
  connect(accent_, &QPushButton::clicked, this,
          [this] { chooseColor(accent_, config_.theme.accent); });
  connect(editorBackground_, &QPushButton::clicked, this,
          [this] { chooseColor(editorBackground_, config_.theme.background); });
  connect(panel_, &QPushButton::clicked, this,
          [this] { chooseColor(panel_, config_.theme.panel); });
  connect(checkerLight_, &QPushButton::clicked, this,
          [this] { chooseColor(checkerLight_, config_.theme.checkerLight); });
  connect(checkerDark_, &QPushButton::clicked, this,
          [this] { chooseColor(checkerDark_, config_.theme.checkerDark); });
  connect(defaultColor_, &QPushButton::clicked, this,
          [this] { chooseColor(defaultColor_, config_.theme.defaultColor); });
  tabs_->addTab(themePage, QStringLiteral("外观"));

  // OCR / paths.
  auto *dataPage = new QWidget(tabs_);
  auto *dataForm = new QFormLayout(dataPage);
  ocrLanguages_ = new QLineEdit(dataPage);
  ocrLanguages_->setPlaceholderText(QStringLiteral("eng 或 eng+chi_sim"));
  auto *dirRow = new QWidget(dataPage);
  auto *dirLayout = new QHBoxLayout(dirRow);
  dirLayout->setContentsMargins(0, 0, 0, 0);
  screenshotDir_ = new QLineEdit(dirRow);
  auto *browse = new QPushButton(QStringLiteral("…"), dirRow);
  dirLayout->addWidget(screenshotDir_, 1);
  dirLayout->addWidget(browse);
  dataForm->addRow(QStringLiteral("OCR 语言"), ocrLanguages_);
  dataForm->addRow(QStringLiteral("截图目录"), dirRow);
  autostart_ = new QCheckBox(QStringLiteral("登录时启动托盘"), dataPage);
  autostart_->setToolTip(QStringLiteral(
      "写入 XDG autostart；若 Niri 不读取 autostart，请改用 Niri 的 spawn-at-startup"));
  dataForm->addRow(QStringLiteral("开机启动"), autostart_);
  connect(browse, &QPushButton::clicked, this, [this] {
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择截图目录"), screenshotDir_->text());
    if (!dir.isEmpty())
      screenshotDir_->setText(dir);
  });
  auto *path = new QLabel(config_.path, dataPage);
  path->setTextInteractionFlags(Qt::TextSelectableByMouse);
  path->setWordWrap(true);
  dataForm->addRow(QStringLiteral("配置文件"), path);
  tabs_->addTab(dataPage, QStringLiteral("截图 / 数据"));

  auto *bottom = new QHBoxLayout;
  pathLabel_ = new QLabel(config_.path, this);
  pathLabel_->setStyleSheet(QStringLiteral("color: #7f8a9a;"));
  pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  pathLabel_->setWordWrap(true);
  auto *openConfig = new QPushButton(QStringLiteral("打开配置文件"), this);
  auto *reset = new QPushButton(QStringLiteral("恢复默认"), this);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                           QDialogButtonBox::Cancel,
                                       this);
  saveButton_ = buttons->button(QDialogButtonBox::Save);
  saveButton_->setText(QStringLiteral("保存"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
  bottom->addWidget(pathLabel_, 1);
  bottom->addWidget(openConfig);
  bottom->addWidget(reset);
  bottom->addWidget(buttons);
  layout->addLayout(bottom);
  connect(openConfig, &QPushButton::clicked, this,
          &SettingsWindow::openConfigFile);
  connect(reset, &QPushButton::clicked, this, &SettingsWindow::resetUi);
  connect(buttons, &QDialogButtonBox::accepted, this,
          &SettingsWindow::saveConfig);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  if (!config_.writable()) {
    saveButton_->setEnabled(false);
    pathLabel_->setText(config_.path + QStringLiteral("（只读：配置版本过新或损坏）"));
  }
}

void SettingsWindow::refreshColorButton(QPushButton *button, const QColor &color) {
  if (!button)
    return;
  button->setText(color.name(QColor::HexRgb).toUpper());
  const bool dark = color.lightness() < 128;
  button->setStyleSheet(
      QStringLiteral("QPushButton { background: %1; color: %2; "
                     "border: 1px solid #4a5363; border-radius: 6px; "
                     "padding: 5px 8px; }")
          .arg(color.name(), dark ? QStringLiteral("#ffffff")
                                  : QStringLiteral("#111111")));
}

void SettingsWindow::chooseColor(QPushButton *button, QColor &color) {
  const QColor chosen = QColorDialog::getColor(color, this);
  if (!chosen.isValid())
    return;
  color = chosen;
  refreshColorButton(button, color);
}

QStringList SettingsWindow::checkedIds(const QListWidget *list) const {
  QStringList ids;
  for (int row = 0; row < list->count(); ++row) {
    const QListWidgetItem *item = list->item(row);
    if (item->checkState() == Qt::Checked)
      ids.push_back(item->data(Qt::UserRole).toString());
  }
  return ids;
}

void SettingsWindow::setCheckedIds(QListWidget *list, const QStringList &ids) {
  for (int row = 0; row < list->count(); ++row) {
    QListWidgetItem *item = list->item(row);
    item->setCheckState(
        ids.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked
                                                           : Qt::Unchecked);
  }
}

void SettingsWindow::loadConfig() {
  setCheckedIds(tools_, config_.tools);
  setCheckedIds(commands_, config_.commands);
  colors_->clear();
  for (const QString &name : config_.palette.colors) {
    const QColor color(name);
    auto *item = new QListWidgetItem(name.toUpper(), colors_);
    item->setData(Qt::UserRole, name);
    item->setBackground(color);
    item->setForeground(color.lightness() < 128 ? Qt::white : Qt::black);
  }
  QStringList widths;
  for (const int width : config_.palette.widths)
    widths.push_back(QString::number(width));
  widths_->setText(widths.join(QStringLiteral(", ")));
  canvasWidth_->setValue(config_.canvas.width);
  canvasHeight_->setValue(config_.canvas.height);
  canvasPadding_->setValue(config_.canvas.padding);
  background_->setCurrentIndex(backgroundIndex(config_));
  refreshColorButton(backgroundColor_, config_.canvas.backgroundColor);
  refreshColorButton(accent_, config_.theme.accent);
  refreshColorButton(editorBackground_, config_.theme.background);
  refreshColorButton(panel_, config_.theme.panel);
  refreshColorButton(checkerLight_, config_.theme.checkerLight);
  refreshColorButton(checkerDark_, config_.theme.checkerDark);
  refreshColorButton(defaultColor_, config_.theme.defaultColor);
  defaultWidth_->setValue(qRound(config_.theme.defaultWidth));
  font_->setCurrentFont(QFont(config_.theme.fontFamily));
  ocrLanguages_->setText(config_.ocrLanguages);
  screenshotDir_->setText(config_.screenshotDir);
  autostart_->setChecked(isAutostartEnabled());
}

void SettingsWindow::resetUi() {
  const AppConfig defaults = AppConfig::defaults();
  setCheckedIds(tools_, defaults.tools);
  setCheckedIds(commands_, defaults.commands);
  colors_->clear();
  for (const QString &name : defaults.palette.colors) {
    const QColor color(name);
    auto *item = new QListWidgetItem(name.toUpper(), colors_);
    item->setData(Qt::UserRole, name);
    item->setBackground(color);
    item->setForeground(color.lightness() < 128 ? Qt::white : Qt::black);
  }
  QStringList widths;
  for (const int width : defaults.palette.widths)
    widths.push_back(QString::number(width));
  widths_->setText(widths.join(QStringLiteral(", ")));
  canvasWidth_->setValue(defaults.canvas.width);
  canvasHeight_->setValue(defaults.canvas.height);
  canvasPadding_->setValue(defaults.canvas.padding);
  background_->setCurrentIndex(backgroundIndex(defaults));
  config_.canvas.backgroundColor = defaults.canvas.backgroundColor;
  config_.theme = defaults.theme;
  refreshColorButton(backgroundColor_, config_.canvas.backgroundColor);
  refreshColorButton(accent_, config_.theme.accent);
  refreshColorButton(editorBackground_, config_.theme.background);
  refreshColorButton(panel_, config_.theme.panel);
  refreshColorButton(checkerLight_, config_.theme.checkerLight);
  refreshColorButton(checkerDark_, config_.theme.checkerDark);
  refreshColorButton(defaultColor_, config_.theme.defaultColor);
  defaultWidth_->setValue(qRound(defaults.theme.defaultWidth));
  font_->setCurrentFont(QFont(defaults.theme.fontFamily));
  ocrLanguages_->clear();
  screenshotDir_->clear();
  autostart_->setChecked(false);
}

void SettingsWindow::addColor() {
  const QColor color = QColorDialog::getColor(Qt::white, this);
  if (!color.isValid())
    return;
  auto *item = new QListWidgetItem(color.name(QColor::HexRgb).toUpper(), colors_);
  item->setData(Qt::UserRole, color.name(QColor::HexRgb));
  item->setBackground(color);
  item->setForeground(color.lightness() < 128 ? Qt::white : Qt::black);
}

void SettingsWindow::removeColor() {
  delete colors_->takeItem(colors_->currentRow());
}

void SettingsWindow::openConfigFile() {
  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(config_.path))) {
    QMessageBox::warning(this, QStringLiteral("无法打开"),
                         QStringLiteral("请手动打开：%1").arg(config_.path));
  }
}

void SettingsWindow::saveConfig() {
  const QStringList tools = checkedIds(tools_);
  if (tools.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("设置"),
                         QStringLiteral("至少保留一个工具"));
    return;
  }
  const QStringList commands = checkedIds(commands_);
  if (commands.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("设置"),
                         QStringLiteral("至少保留一个工具栏命令"));
    return;
  }
  QStringList colors;
  for (int row = 0; row < colors_->count(); ++row)
    colors.push_back(colors_->item(row)->data(Qt::UserRole).toString());
  if (colors.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("设置"),
                         QStringLiteral("调色板至少需要一个颜色"));
    return;
  }
  QVector<int> widths;
  for (const QString &part : widths_->text().split(QLatin1Char(','))) {
    bool ok = false;
    const int width = part.trimmed().toInt(&ok);
    if (!ok || width <= 0) {
      QMessageBox::warning(this, QStringLiteral("设置"),
                           QStringLiteral("线宽必须是正整数，用逗号分隔"));
      return;
    }
    widths.push_back(width);
  }
  if (widths.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("设置"),
                         QStringLiteral("至少保留一个线宽"));
    return;
  }
  config_.tools = tools;
  config_.commands = commands;
  config_.palette.colors = colors;
  config_.palette.widths = widths;
  config_.canvas.width = canvasWidth_->value();
  config_.canvas.height = canvasHeight_->value();
  config_.canvas.padding = canvasPadding_->value();
  config_.canvas.background = backgroundKey(background_->currentIndex());
  config_.theme.accent = QColor(accent_->text());
  config_.theme.background = QColor(editorBackground_->text());
  config_.theme.panel = QColor(panel_->text());
  config_.theme.checkerLight = QColor(checkerLight_->text());
  config_.theme.checkerDark = QColor(checkerDark_->text());
  config_.theme.defaultColor = QColor(defaultColor_->text());
  config_.theme.defaultWidth = defaultWidth_->value();
  config_.theme.fontFamily = font_->currentFont().family();
  config_.ocrLanguages = ocrLanguages_->text().trimmed();
  config_.screenshotDir = screenshotDir_->text().trimmed();
  QString error;
  if (!config_.save(&error)) {
    QMessageBox::critical(this, QStringLiteral("保存失败"), error);
    return;
  }
  if (!setAutostartEnabled(autostart_->isChecked(), &error)) {
    QMessageBox::warning(this, QStringLiteral("开机启动"), error);
  }
  QMessageBox::information(this, QStringLiteral("设置"),
                           QStringLiteral("已保存到：\n%1\n\n新建编辑器会使用新配置。")
                               .arg(config_.path));
  accept();
}
