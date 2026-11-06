#include "tool-smoke.hpp"

#include "editor/annotation-document.hpp"
#include "editor/editor-canvas.hpp"
#include "editor/tool-bar.hpp"
#include "editor/tool-registry.hpp"
#include "editor/tools/builtin-tools.hpp"

#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRectF>

#include <cmath>

namespace {
bool check(bool condition, const char *message, QString &error) {
  if (condition)
    return true;
  error = QString::fromLatin1(message);
  return false;
}
} // namespace

bool runEditorToolSmoke(QString &error) {
  ToolRegistry registry;
  registerBuiltinTools(registry);
  if (!check(registry.tools().size() == 11, "Expected 11 built-in tools", error))
    return false;
  if (!check(registry.find(QStringLiteral("select")) != nullptr,
             "Select tool missing", error))
    return false;
  if (!check(registry.defaultToolId() == QStringLiteral("select"),
             "Select must be the default tool", error))
    return false;

  AnnotationDocument document;
  document.pushUndo();
  Annotation rect;
  rect.kind = Annotation::Kind::Rectangle;
  rect.start = {0, 0};
  rect.end = {40, 30};
  document.add(rect);
  if (!check(document.annotations().size() == 1, "Document add failed", error))
    return false;
  document.undo();
  if (!check(document.annotations().isEmpty(), "Document undo failed", error))
    return false;
  document.redo();
  if (!check(document.annotations().size() == 1, "Document redo failed", error))
    return false;

  EditorCanvas canvas;
  canvas.setSelection(QRectF(10, 10, 300, 200));
  canvas.setTool(QStringLiteral("arrow"));
  ToolEvent press;
  press.pos = {20, 20};
  ToolEvent move = press;
  move.pos = {100, 80};
  ToolEvent release = move;
  if (!check(canvas.press(press), "Arrow press not consumed", error))
    return false;
  if (!check(canvas.preview().has_value(), "Arrow preview missing", error))
    return false;
  canvas.move(move);
  canvas.release(release);
  if (!check(canvas.document().annotations().size() == 1,
             "Arrow not committed", error))
    return false;
  if (canvas.document().annotations().first().kind != Annotation::Kind::Arrow)
    return check(false, "Wrong annotation kind", error);

  canvas.setTool(QStringLiteral("pen"));
  canvas.press(press);
  canvas.move(move);
  if (!check(canvas.preview().has_value(), "Pen preview missing", error))
    return false;
  const int before = canvas.document().annotations().size();
  canvas.setTool(QStringLiteral("mosaic"));
  if (!check(!canvas.preview().has_value(),
             "Tool switch left a stale preview", error))
    return false;
  if (!check(canvas.document().annotations().size() == before,
             "Tool switch mutated the document", error))
    return false;


  const QVector<QPair<QString, Annotation::Kind>> drawTools{
      {QStringLiteral("arrow"), Annotation::Kind::Arrow},
      {QStringLiteral("line"), Annotation::Kind::Line},
      {QStringLiteral("rectangle"), Annotation::Kind::Rectangle},
      {QStringLiteral("ellipse"), Annotation::Kind::Ellipse},
      {QStringLiteral("pen"), Annotation::Kind::Freehand},
      {QStringLiteral("highlighter"), Annotation::Kind::Highlighter},
      {QStringLiteral("mosaic"), Annotation::Kind::Redaction},
      {QStringLiteral("blur"), Annotation::Kind::Blur},
  };
  for (const auto &[id, expected] : drawTools) {
    canvas.document().clear();
    canvas.setTool(id);
    canvas.press(press);
    canvas.move(move);
    canvas.release(release);
    if (!check(canvas.document().annotations().size() == 1,
               "Tool did not commit exactly one annotation", error))
      return false;
    if (!check(canvas.document().annotations().first().kind == expected,
               "Tool produced the wrong annotation kind", error))
      return false;
  }

  canvas.document().clear();
  canvas.setTool(QStringLiteral("marker"));
  canvas.press(press);
  if (!check(canvas.document().annotations().size() == 1 &&
                 canvas.document().annotations().first().number == 1,
             "Marker numbering failed", error))
    return false;

  canvas.document().clear();
  canvas.setTool(QStringLiteral("rectangle"));
  canvas.press(press);
  canvas.move(move);
  canvas.release(release);
  canvas.setTool(QStringLiteral("select"));
  ToolEvent movePress = press;
  movePress.pos = {20, 20};
  ToolEvent moveEnd = movePress;
  moveEnd.pos = {60, 50};
  if (!check(canvas.press(movePress), "Select tool did not hit a layer", error))
    return false;
  canvas.move(moveEnd);
  canvas.release(moveEnd);
  if (!check(canvas.document().annotations().first().start == QPointF(60, 50),
             "Select tool did not move the layer", error))
    return false;

  bool textRequested = false;
  canvas.setTextCallback([&](const QPointF &) { textRequested = true; });
  canvas.setTool(QStringLiteral("text"));
  canvas.press(press);
  if (!check(textRequested, "Text tool did not request an editor", error))
    return false;

  ToolBar bar(registry);
  bar.setCurrentTool(QStringLiteral("pen"));
  bar.setColors(QColor(QStringLiteral("#01a2ff")), true);
  bar.setHistory(true, true);
  const QRectF viewport(0, 0, 900, 700);
  const QRectF selection(120, 120, 500, 300);
  const QVector<ToolButton> buttons = bar.buttons(viewport, selection);
  if (!check(buttons.size() >= 18, "Toolbar lost buttons", error))
    return false;
  ToolButton pen;
  for (const ToolButton &button : buttons) {
    if (button.id == QStringLiteral("pen")) {
      pen = button;
      break;
    }
  }
  if (!check(!pen.rect.isEmpty(), "Pen toolbar button missing", error))
    return false;
  const ToolBarHit hit =
      bar.hitTest(viewport, selection, pen.rect.center());
  if (!check(hit.valid && hit.isTool && hit.id == QStringLiteral("pen"),
             "Toolbar hit test failed", error))
    return false;
  bar.setPaletteOpen(true);
  bool foundSwatch = false;
  for (const ToolButton &button : bar.buttons(viewport, selection)) {
    if (button.id == QStringLiteral("color-0"))
      foundSwatch = true;
  }
  if (!check(foundSwatch, "Palette buttons missing", error))
    return false;

  ToolBar paletteBar(registry);
  paletteBar.setPalette({QStringLiteral("#00ff00"), QStringLiteral("#ff00ff")},
                        {3, 9});
  paletteBar.setColors(QColor(QStringLiteral("#00ff00")), false);
  paletteBar.setWidth(9);
  paletteBar.setPaletteOpen(true);
  bool colorSelected = false;
  bool widthSelected = false;
  for (const ToolButton &button : paletteBar.buttons(viewport, selection)) {
    if (button.id == QStringLiteral("color-0"))
      colorSelected = button.selected;
    if (button.id == QStringLiteral("size-9"))
      widthSelected = button.selected;
  }
  if (!check(colorSelected, "Colour selection is not marked", error))
    return false;
  if (!check(widthSelected, "Width selection is not marked", error))
    return false;
  const QRectF barRect = paletteBar.rect(viewport, selection);
  if (!check(paletteBar.contains(viewport, selection,
                                 QPointF(barRect.left() + 2,
                                         barRect.center().y())),
             "Toolbar gaps are not part of the hand-cursor area", error))
    return false;

  QImage image(400, 300, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  canvas.paintVectors(painter, QPointF(0, 0));
  painter.end();
  if (!check(!image.isNull(), "Vector paint failed", error))
    return false;
  return true;
}
