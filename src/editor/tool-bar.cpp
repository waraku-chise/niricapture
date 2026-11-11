#include "editor/tool-bar.hpp"

#include "editor/tool-registry.hpp"
#include "i18n.hpp"
#include "icons.hpp"

#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr qreal kButton = 34.0;
constexpr qreal kGap = 4.0;
constexpr qreal kGroupGap = 6.0;
constexpr qreal kHeight = 38.0;
constexpr qreal kPalettePad = 8.0;
constexpr qreal kWidthControlWidth = 150.0;
constexpr qreal kColorCell = 24.0;
constexpr qreal kColorGap = 2.0;
constexpr int kColorColumns = 9;

QVector<QString> commandIds() {
  return {QStringLiteral("palette"), QStringLiteral("font"),
          QStringLiteral("undo"),    QStringLiteral("redo"),
          QStringLiteral("ocr"),     QStringLiteral("copy"),
          QStringLiteral("save"),    QStringLiteral("close")};
}

QString commandIcon(const QString &id) {
  if (id == QStringLiteral("palette"))
    return QStringLiteral("palette");
  if (id == QStringLiteral("font"))
    return QStringLiteral("tool-text");
  if (id == QStringLiteral("ocr"))
    return QStringLiteral("tool-ocr");
  return id;
}

QString commandTooltip(const QString &id) {
  if (id == QStringLiteral("palette"))
    return ui("Colour / width", "颜色 / 线宽");
  if (id == QStringLiteral("font"))
    return ui("Text font", "文字字体");
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
  paletteColors_ = {QStringLiteral("#000000"), QStringLiteral("#4a4a4a"), QStringLiteral("#7f1d1d"), QStringLiteral("#dc2626"), QStringLiteral("#ea580c"), QStringLiteral("#eab308"), QStringLiteral("#16a34a"), QStringLiteral("#0d9488"), QStringLiteral("#2563eb"), QStringLiteral("#6b7280"), QStringLiteral("#9ca3af"), QStringLiteral("#9f1239"), QStringLiteral("#f87171"), QStringLiteral("#fb923c"), QStringLiteral("#fde047"), QStringLiteral("#4ade80"), QStringLiteral("#2dd4bf"), QStringLiteral("#60a5fa"), QStringLiteral("#cbd5e1"), QStringLiteral("#ffffff"), QStringLiteral("#92400e"), QStringLiteral("#fca5a5"), QStringLiteral("#fdba74"), QStringLiteral("#fef08a"), QStringLiteral("#86efac"), QStringLiteral("#99f6e4"), QStringLiteral("#93c5fd"), QStringLiteral("#7c3aed"), QStringLiteral("#a855f7"), QStringLiteral("#db2777"), QStringLiteral("#ec4899"), QStringLiteral("#f472b6"), QStringLiteral("#f9a8d4"), QStringLiteral("#a16207"), QStringLiteral("#78350f"), QStringLiteral("#1e3a8a")};
  paletteWidths_ = {2, 4, 8, 12};
}

void ToolBar::setCommands(const QStringList &commands) {
  if (!commands.isEmpty())
    commands_ = commands;
}

void ToolBar::setTheme(const Md3Tokens &tokens) { tokens_ = tokens; }

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
  if (dock_ == Dock::Top) {
    const qreal dockX = std::clamp(
        (viewport.width() - width) / 2.0, 8.0,
        std::max(8.0, viewport.width() - width - 8.0));
    return {dockX, viewport.top() + 32, width, kHeight};
  }
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
  const int columns =
      std::clamp(static_cast<int>(paletteColors_.size()), 1, kColorColumns);
  const int rows =
      std::max(1, static_cast<int>(std::ceil(paletteColors_.size() /
                                             static_cast<qreal>(columns))));
  const qreal gridWidth =
      columns * kColorCell + std::max(0, columns - 1) * kColorGap;
  const qreal width =
      kPalettePad + kWidthControlWidth + kPalettePad + 36 + kPalettePad +
      gridWidth + kPalettePad;
  const qreal height =
      std::max<qreal>(118.0, 16.0 + rows * (kColorCell + kColorGap));
  const qreal x = std::clamp(bar.center().x() - width / 2.0, 8.0,
                             std::max(8.0, viewport.width() - width - 8.0));
  qreal y = bar.bottom() + 6;
  if (y + height > viewport.height() - 8)
    y = bar.top() - height - 6;
  return {x, std::max<qreal>(8, y), width, height};
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
    ToolButton lineStyle;
    lineStyle.rect = {panel.left() + kPalettePad, panel.top() + kPalettePad,
                      kWidthControlWidth, 36};
    lineStyle.id = QStringLiteral("line-style");
    lineStyle.tooltip = ui("Line style", "线形");
    lineStyle.selected = true;
    result.push_back(lineStyle);

    for (int index = 0; index < paletteWidths_.size(); ++index) {
      ToolButton button;
      button.rect = {panel.left() + kPalettePad + index * 32,
                     panel.top() + 52, 24, 24};
      button.id = QStringLiteral("size-%1").arg(paletteWidths_.at(index));
      button.tooltip = QString::number(paletteWidths_.at(index));
      button.selected =
          qFuzzyCompare(width_ + 1.0, paletteWidths_.at(index) + 1.0);
      button.hovered = button.id == hovered_;
      button.pressed = button.id == pressed_;
      result.push_back(button);
    }

    ToolButton custom;
    custom.rect = {lineStyle.rect.right() + kPalettePad,
                   panel.top() + kPalettePad, 36, 36};
    custom.id = QStringLiteral("custom-color");
    custom.tooltip = ui("Custom colour", "自定义颜色");
    custom.swatch = color_;
    result.push_back(custom);

    const qreal gridLeft = custom.rect.right() + kPalettePad;
    const int columns =
        std::clamp(static_cast<int>(paletteColors_.size()), 1, kColorColumns);
    for (int index = 0; index < paletteColors_.size(); ++index) {
      const int column = index % columns;
      const int row = index / columns;
      ToolButton button;
      button.rect = {gridLeft + column * (kColorCell + kColorGap),
                     panel.top() + kPalettePad +
                         row * (kColorCell + kColorGap),
                     kColorCell, kColorCell};
      button.id = QStringLiteral("color-%1").arg(index);
      button.swatch = QColor(paletteColors_.at(index));
      button.tooltip = button.swatch.name(QColor::HexRgb).toUpper();
      button.selected = button.swatch.rgb() == color_.rgb();
      button.hovered = button.id == hovered_;
      button.pressed = button.id == pressed_;
      result.push_back(button);
    }

    ToolButton fill;
    fill.rect = {panel.left() + kPalettePad, panel.top() + 84,
                 kWidthControlWidth, 24};
    fill.id = QStringLiteral("fill");
    fill.tooltip = filled_ ? ui("Fill: on", "填充：开")
                           : ui("Fill: off", "填充：关");
    fill.active = filled_;
    fill.hovered = fill.id == hovered_;
    fill.pressed = fill.id == pressed_;
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
  painter.setPen(QPen(tokens_.outlineVariant, 1));
  painter.setBrush(tokens_.surfaceContainerHigh);
  painter.drawRoundedRect(bar.adjusted(-4, -4, 4, 4), 14, 14);
  if (paletteOpen_) {
    painter.setPen(QPen(tokens_.outlineVariant, 1));
    painter.setBrush(tokens_.surfaceContainerHigh);
    painter.drawRoundedRect(paletteRect(viewport, selection), 14, 14);
  }

  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  font.setPixelSize(12);
  font.setWeight(QFont::Medium);
  const QVector<ToolButton> all = buttons(viewport, selection);
  for (const ToolButton &button : all) {
    const bool paletteSwatch = button.id.startsWith(QStringLiteral("color-"));
    if (!paletteSwatch && (button.hovered || button.active || button.pressed)) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(button.active
                           ? tokens_.primaryContainer
                           : button.pressed ? tokens_.surfaceContainerHighest
                                            : tokens_.hover);
      painter.drawRoundedRect(button.rect, 9, 9);
    }
    if (button.id == QStringLiteral("line-style")) {
      painter.setPen(QPen(tokens_.outlineVariant, 1));
      painter.setBrush(tokens_.surfaceContainer);
      painter.drawRoundedRect(button.rect, 8, 8);
      Qt::PenStyle penStyle = Qt::SolidLine;
      switch (strokeStyle_) {
      case StrokeStyle::Dashed:
        penStyle = Qt::DashLine;
        break;
      case StrokeStyle::Dotted:
        penStyle = Qt::DotLine;
        break;
      case StrokeStyle::DashDot:
        penStyle = Qt::DashDotLine;
        break;
      case StrokeStyle::Solid:
        break;
      }
      painter.setPen(QPen(tokens_.onSurface, 2, penStyle, Qt::RoundCap));
      painter.drawLine(QPointF(button.rect.left() + 14,
                               button.rect.center().y()),
                       QPointF(button.rect.right() - 32,
                               button.rect.center().y()));
      painter.setPen(QPen(tokens_.onSurfaceVariant, 1.5));
      const QPointF chevron(button.rect.right() - 18,
                            button.rect.center().y());
      painter.drawLine(chevron + QPointF(-4, -3), chevron + QPointF(0, 1));
      painter.drawLine(chevron + QPointF(0, 1), chevron + QPointF(4, -3));
      continue;
    }
    if (button.id.startsWith(QStringLiteral("size-"))) {
      const qreal diameter =
          std::clamp(button.tooltip.toDouble() * 1.5, 4.0, 18.0);
      painter.setPen(QPen(tokens_.onSurfaceVariant, 1));
      painter.setBrush(tokens_.onSurface);
      painter.drawEllipse(button.rect.center(), diameter / 2.0, diameter / 2.0);
      if (button.selected) {
        painter.setPen(QPen(tokens_.primary, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(button.rect.adjusted(1, 1, -1, -1));
      }
      continue;
    }
    if (button.id == QStringLiteral("custom-color")) {
      painter.save();
      QPainterPath clip;
      clip.addRoundedRect(button.rect, 6, 6);
      painter.setClipPath(clip);
      painter.fillRect(button.rect, button.swatch);
      QPainterPath rainbow;
      rainbow.moveTo(button.rect.right() - 15, button.rect.bottom());
      rainbow.lineTo(button.rect.right(), button.rect.bottom());
      rainbow.lineTo(button.rect.right(), button.rect.bottom() - 15);
      rainbow.closeSubpath();
      QLinearGradient spectrum(button.rect.bottomLeft(),
                               button.rect.topRight());
      spectrum.setColorAt(0.0, QColor(QStringLiteral("#ff0055")));
      spectrum.setColorAt(0.35, QColor(QStringLiteral("#ffcc00")));
      spectrum.setColorAt(0.65, QColor(QStringLiteral("#33dd88")));
      spectrum.setColorAt(1.0, QColor(QStringLiteral("#3388ff")));
      painter.fillPath(rainbow, spectrum);
      painter.restore();
      painter.setPen(QPen(tokens_.outline, 1));
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(button.rect, 8, 8);
      continue;
    }
    if (button.id.startsWith(QStringLiteral("color-"))) {
      painter.setPen(QPen(tokens_.outlineVariant, 1));
      painter.setBrush(button.swatch);
      painter.drawRoundedRect(button.rect.adjusted(1, 1, -1, -1), 4, 4);
      if (button.selected) {
        painter.setPen(QPen(tokens_.primary, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(button.rect.adjusted(0, 0, -1, -1), 4, 4);
      }
      continue;
    }
    if (button.id == QStringLiteral("fill")) {
      if (button.active) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(tokens_.primaryContainer);
        painter.drawRoundedRect(button.rect, 9, 9);
      }
      QColor fillText = button.active ? tokens_.onPrimaryContainer
                                      : tokens_.onSurface;
      if (!button.enabled)
        fillText.setAlpha(110);
      painter.setPen(fillText);
      painter.setFont(font);
      painter.drawText(button.rect, Qt::AlignCenter, button.tooltip);
      continue;
    }
    if (button.id == QStringLiteral("palette")) {
      painter.setPen(QPen(tokens_.outline, 1));
      painter.setBrush(button.swatch);
      painter.drawEllipse(
          QRectF(button.rect.center().x() - 8, button.rect.center().y() - 8, 16,
                 16));
      continue;
    }
    QColor iconColor = button.active ? tokens_.onPrimaryContainer
                                     : tokens_.onSurfaceVariant;
    if (!button.enabled)
      iconColor.setAlpha(110);
    drawToolbarIcon(painter, button.rect, button.icon, {}, iconColor);
  }

  for (const ToolButton &button : all) {
    if (!button.hovered ||
        button.id.startsWith(QStringLiteral("color-")) ||
        button.id == QStringLiteral("custom-color") ||
        button.id == QStringLiteral("fill") ||
        button.id.startsWith(QStringLiteral("size-")))
      continue;
    const QString text =
        button.tooltip.section(QStringLiteral(" · "), 0, 0).trimmed();
    if (text.isEmpty())
      continue;
    const QFontMetricsF metrics(font);
    const qreal width = std::min(
        metrics.horizontalAdvance(text) + 18.0, viewport.width() - 8.0);
    const qreal height = 24.0;
    qreal x = button.rect.center().x() - width / 2.0;
    x = std::clamp(x, 4.0, std::max(4.0, viewport.width() - width - 4.0));
    qreal y = button.rect.top() - height - 5.0;
    if (y < 4.0)
      y = button.rect.bottom() + 5.0;
    const QRectF bubble(x, y, width, height);
    painter.setPen(QPen(tokens_.outlineVariant, 1));
    painter.setBrush(tokens_.surfaceContainerHigh);
    painter.drawRoundedRect(bubble, 8, 8);
    painter.setPen(tokens_.onSurface);
    painter.setFont(font);
    painter.drawText(bubble, Qt::AlignCenter, text);
    break;
  }
}
