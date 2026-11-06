#include "editor/tool-bar.hpp"

#include "editor/tool-registry.hpp"
#include "i18n.hpp"
#include "icons.hpp"

#include <QFont>
#include <QFontDatabase>
#include <QPainter>

#include <algorithm>
#include <array>

namespace {
constexpr qreal kButton = 34.0;
constexpr qreal kGap = 4.0;
constexpr qreal kGroupGap = 6.0;
constexpr qreal kHeight = 38.0;

QVector<QString> commandIds() {
  return {QStringLiteral("palette"), QStringLiteral("undo"),
          QStringLiteral("redo"),    QStringLiteral("ocr"),
          QStringLiteral("copy"),    QStringLiteral("save"),
          QStringLiteral("close")};
}

QString commandIcon(const QString &id) {
  if (id == QStringLiteral("palette"))
    return QStringLiteral("palette");
  if (id == QStringLiteral("ocr"))
    return QStringLiteral("tool-ocr");
  return id;
}

QString commandTooltip(const QString &id) {
  if (id == QStringLiteral("palette"))
    return ui("Colour / width", "颜色 / 线宽");
  if (id == QStringLiteral("undo"))
    return ui("Undo", "撤销");
  if (id == QStringLiteral("redo"))
    return ui("Redo", "重做");
  if (id == QStringLiteral("ocr"))
    return ui("OCR", "OCR");
  if (id == QStringLiteral("copy"))
    return ui("Copy", "复制");
  if (id == QStringLiteral("save"))
    return ui("Save", "保存");
  return ui("Close", "关闭");
}
} // namespace

ToolBar::ToolBar(const ToolRegistry &registry) : registry_(registry) {
  commands_ = commandIds();
  paletteColors_ = {QStringLiteral("#ff375f"), QStringLiteral("#ff9f0a"),
                    QStringLiteral("#ffd60a"), QStringLiteral("#30d158"),
                    QStringLiteral("#0a84ff"), QStringLiteral("#bf5af2"),
                    QStringLiteral("#ffffff"), QStringLiteral("#000000")};
  paletteWidths_ = {2, 4, 8, 12};
}

void ToolBar::setCommands(const QStringList &commands) {
  if (!commands.isEmpty())
    commands_ = commands;
}

void ToolBar::setTheme(const QColor &panel, const QColor &border,
                       const QColor &accent) {
  if (panel.isValid())
    panelColor_ = panel;
  if (border.isValid())
    borderColor_ = border;
  if (accent.isValid())
    accentColor_ = accent;
}

void ToolBar::setPalette(const QStringList &colors, const QVector<int> &widths) {
  if (!colors.isEmpty())
    paletteColors_ = colors;
  if (!widths.isEmpty())
    paletteWidths_ = widths;
}

QColor ToolBar::colorAt(int index) const {
  if (index < 0 || index >= paletteColors_.size())
    return {};
  return QColor(paletteColors_.at(index));
}

int ToolBar::widthAt(int index) const {
  if (index < 0 || index >= paletteWidths_.size())
    return 4;
  return paletteWidths_.at(index);
}

void ToolBar::setCurrentTool(const QString &id) { currentTool_ = id; }
void ToolBar::setColors(const QColor &color, bool filled) {
  color_ = color;
  filled_ = filled;
}

void ToolBar::setWidth(qreal width) { width_ = width; }
void ToolBar::setHistory(bool canUndo, bool canRedo) {
  canUndo_ = canUndo;
  canRedo_ = canRedo;
}
void ToolBar::setHovered(const QString &id) { hovered_ = id; }
void ToolBar::setPressed(const QString &id) { pressed_ = id; }

QRectF ToolBar::rect(const QRectF &viewport, const QRectF &selection) const {
  const QStringList tools = registry_.visibleToolIds();
  int count = static_cast<int>(tools.size()) +
              static_cast<int>(commands_.size());
  int groups = 0;
  ToolGroup previous = ToolGroup::Select;
  for (const QString &id : tools) {
    IEditorTool *tool = registry_.find(id);
    if (!tool)
      continue;
    if (tool->group() != previous) {
      ++groups;
      previous = tool->group();
    }
  }
  const qreal width = count * kButton + (count - 1) * kGap + groups * kGroupGap;
  qreal x = selection.center().x() - width / 2.0;
  x = std::clamp(x, 8.0, std::max(8.0, viewport.width() - width - 8.0));
  qreal y = selection.top() - kHeight - 10;
  if (y < 8)
    y = selection.bottom() + 10;
  if (y + kHeight > viewport.height() - 8)
    y = std::clamp(selection.top() + 8, 8.0,
                   std::max(8.0, viewport.height() - kHeight - 8));
  return {x, y, width, kHeight};
}

QRectF ToolBar::paletteRect(const QRectF &viewport,
                            const QRectF &selection) const {
  const QRectF bar = rect(viewport, selection);
  const qreal width =
      std::max<qreal>(300, 16 + paletteWidths_.size() * 58);
  const qreal x = std::clamp(bar.center().x() - width / 2.0, 8.0,
                             std::max(8.0, viewport.width() - width - 8.0));
  qreal y = bar.bottom() + 6;
  if (y + 76 > viewport.height() - 8)
    y = bar.top() - 82;
  return {x, std::max<qreal>(8, y), width, 76};
}

QVector<ToolButton> ToolBar::buttons(const QRectF &viewport,
                                     const QRectF &selection) const {
  QVector<ToolButton> result;
  const QRectF bar = rect(viewport, selection);
  qreal x = bar.left();
  ToolGroup previous = ToolGroup::Select;
  bool first = true;
  for (const QString &toolId : registry_.visibleToolIds()) {
    IEditorTool *tool = registry_.find(toolId);
    if (!tool)
      continue;
    if (!first && tool->group() != previous)
      x += kGroupGap;
    previous = tool->group();
    first = false;
    ToolButton button;
    button.rect = {x, bar.top(), kButton, bar.height()};
    button.id = tool->id();
    button.icon = tool->icon();
    button.tooltip = tool->tooltip();
    button.isTool = true;
    button.active = tool->id() == currentTool_;
    button.hovered = tool->id() == hovered_;
    button.pressed = tool->id() == pressed_;
    result.push_back(button);
    x += kButton + kGap;
  }
  for (const QString &id : commands_) {
    ToolButton button;
    button.rect = {x, bar.top(), kButton, bar.height()};
    button.id = id;
    button.icon = commandIcon(id);
    button.tooltip = commandTooltip(id);
    button.enabled = !((id == QStringLiteral("undo") && !canUndo_) ||
                       (id == QStringLiteral("redo") && !canRedo_));
    button.active = id == QStringLiteral("palette") && paletteOpen_;
    button.hovered = id == hovered_;
    button.pressed = id == pressed_;
    if (id == QStringLiteral("palette"))
      button.swatch = color_;
    result.push_back(button);
    x += kButton + kGap;
  }

  if (paletteOpen_) {
    const QRectF panel = paletteRect(viewport, selection);
    for (int index = 0; index < paletteColors_.size(); ++index) {
      ToolButton button;
      button.rect = {panel.left() + 8 + index * 32, panel.top() + 7, 24, 24};
      button.id = QStringLiteral("color-%1").arg(index);
      button.swatch = QColor(paletteColors_.at(index));
      button.selected = button.swatch.rgb() == color_.rgb();
      button.hovered = button.id == hovered_;
      button.pressed = button.id == pressed_;
      result.push_back(button);
    }
    for (int index = 0; index < paletteWidths_.size(); ++index) {
      ToolButton button;
      button.rect = {panel.left() + 8 + index * 58, panel.top() + 40, 52, 26};
      button.id = QStringLiteral("size-%1").arg(paletteWidths_.at(index));
      button.tooltip = QString::number(paletteWidths_.at(index));
      button.selected =
          qFuzzyCompare(width_ + 1.0, paletteWidths_.at(index) + 1.0);
      result.push_back(button);
    }
    ToolButton fill;
    fill.rect = {panel.left() + 8 + paletteWidths_.size() * 58,
                 panel.top() + 40, 52, 26};
    fill.id = QStringLiteral("fill");
    fill.tooltip = filled_ ? ui("Fill: on", "填充：开")
                           : ui("Fill: off", "填充：关");
    result.push_back(fill);
  }
  return result;
}

bool ToolBar::contains(const QRectF &viewport, const QRectF &selection,
                       const QPointF &local) const {
  if (rect(viewport, selection).adjusted(-4, -4, 4, 4).contains(local))
    return true;
  return paletteOpen_ && paletteRect(viewport, selection).contains(local);
}

ToolBarHit ToolBar::hitTest(const QRectF &viewport, const QRectF &selection,
                            const QPointF &local) const {
  for (const ToolButton &button : buttons(viewport, selection)) {
    if (!button.enabled || !button.rect.contains(local))
      continue;
    return {true, button.isTool, button.id};
  }
  return {};
}

void ToolBar::paint(QPainter &painter, const QRectF &viewport,
                    const QRectF &selection) const {
  const QRectF bar = rect(viewport, selection);
  painter.setPen(QPen(borderColor_, 1));
  painter.setBrush(panelColor_);
  painter.drawRoundedRect(bar.adjusted(-4, -4, 4, 4), 11, 11);
  if (paletteOpen_) {
    painter.setPen(QPen(borderColor_, 1));
    painter.setBrush(panelColor_);
    painter.drawRoundedRect(paletteRect(viewport, selection), 10, 10);
  }

  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  font.setPixelSize(11);
  for (const ToolButton &button : buttons(viewport, selection)) {
    const bool paletteSwatch =
        button.id.startsWith(QStringLiteral("color-")) ||
        button.id.startsWith(QStringLiteral("size-"));
    if (!paletteSwatch && (button.hovered || button.active || button.pressed)) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(button.pressed ? QColor(255, 255, 255, 38)
                                     : button.hovered
                                           ? QColor(255, 255, 255, 22)
                                           : QColor(accentColor_.red(),
                                                    accentColor_.green(),
                                                    accentColor_.blue(), 70));
      painter.drawRoundedRect(button.rect, 7, 7);
    }
    if (button.id.startsWith(QStringLiteral("color-"))) {
      painter.setPen(QPen(QColor(255, 255, 255, 60), 1));
      painter.setBrush(button.swatch);
      painter.drawEllipse(button.rect.adjusted(5, 5, -5, -5));
      if (button.selected) {
        painter.setPen(QPen(accentColor_, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(button.rect.adjusted(1, 1, -1, -1));
      }
      continue;
    }
    if (button.id.startsWith(QStringLiteral("size-"))) {
      const qreal diameter =
          std::clamp(button.tooltip.toDouble() * 1.5, 4.0, 18.0);
      painter.setPen(QPen(QColor(255, 255, 255, 110), 1));
      painter.setBrush(QColor(240, 243, 248));
      painter.drawEllipse(button.rect.center(), diameter / 2.0, diameter / 2.0);
      if (button.selected) {
        painter.setPen(QPen(accentColor_, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(button.rect.adjusted(1, 1, -1, -1));
      }
      continue;
    }
    if (button.id == QStringLiteral("fill")) {
      painter.setPen(button.enabled ? QColor(240, 243, 248)
                                    : QColor(240, 243, 248, 70));
      painter.setFont(font);
      painter.drawText(button.rect, Qt::AlignCenter, button.tooltip);
      continue;
    }
    if (button.id == QStringLiteral("palette")) {
      painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
      painter.setBrush(button.swatch);
      painter.drawEllipse(
          QRectF(button.rect.center().x() - 8, button.rect.center().y() - 8, 16,
                 16));
      continue;
    }
    drawToolbarIcon(painter, button.rect, button.icon, {},
                    button.enabled ? QColor(245, 245, 247)
                                   : QColor(245, 245, 247, 70));
  }
}
