/** @fileoverview Toolbar model, layout and painting (no tool logic). */
#pragma once

#include "editor/tool.hpp"

#include <QColor>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QStringList>

class QPainter;
class ToolRegistry;

struct ToolButton {
  QRectF rect;
  QString id;
  QString icon;
  QString tooltip;
  QColor swatch;
  bool isTool = false;
  bool enabled = true;
  bool active = false;
  bool selected = false;
  bool hovered = false;
  bool pressed = false;
};

struct ToolBarHit {
  bool valid = false;
  bool isTool = false;
  QString id;
};

/** Builds and paints the floating toolbar; tool actions are registry ids. */
class ToolBar {
public:
  explicit ToolBar(const ToolRegistry &registry);

  void setCurrentTool(const QString &id);
  void setCommands(const QStringList &commands);
  void setTheme(const QColor &panel, const QColor &border,
                const QColor &accent);
  void setPalette(const QStringList &colors, const QVector<int> &widths);
  [[nodiscard]] QColor colorAt(int index) const;
  [[nodiscard]] int widthAt(int index) const;
  void setPaletteOpen(bool open) { paletteOpen_ = open; }
  [[nodiscard]] bool paletteOpen() const { return paletteOpen_; }
  void setColors(const QColor &color, bool filled);
  void setWidth(qreal width);
  void setHistory(bool canUndo, bool canRedo);
  void setHovered(const QString &id);
  void setPressed(const QString &id);

  [[nodiscard]] QRectF rect(const QRectF &viewport,
                            const QRectF &selection) const;
  [[nodiscard]] QRectF paletteRect(const QRectF &viewport,
                                   const QRectF &selection) const;
  [[nodiscard]] QVector<ToolButton> buttons(const QRectF &viewport,
                                             const QRectF &selection) const;
  [[nodiscard]] ToolBarHit hitTest(const QRectF &viewport,
                                   const QRectF &selection,
                                   const QPointF &local) const;
  [[nodiscard]] bool contains(const QRectF &viewport,
                              const QRectF &selection,
                              const QPointF &local) const;
  void paint(QPainter &painter, const QRectF &viewport,
             const QRectF &selection) const;

private:
  const ToolRegistry &registry_;
  QString currentTool_;
  QColor color_ = QColor(QStringLiteral("#ff375f"));
  qreal width_ = 4.0;
  bool filled_ = false;
  bool paletteOpen_ = false;
  bool canUndo_ = false;
  bool canRedo_ = false;
  QString hovered_;
  QString pressed_;
  QStringList commands_;
  QStringList paletteColors_;
  QVector<int> paletteWidths_;
  QColor panelColor_ = QColor(17, 19, 24, 246);
  QColor borderColor_ = QColor(255, 255, 255, 30);
  QColor accentColor_ = QColor(124, 196, 255);
};
