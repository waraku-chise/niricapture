#include "tool-smoke.hpp"

#include "editor/annotation-document.hpp"
#include "editor/editor-canvas.hpp"
#include "editor/tool-bar.hpp"
#include "editor/tool-registry.hpp"
#include "editor/tools/builtin-tools.hpp"
#include "capture.hpp"
#include "config.hpp"

#include <QDir>

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
  if (qEnvironmentVariableIsSet("NIRICAPTURE_PALETTE_SNAPSHOT")) {
    const AppConfig defaults = AppConfig::defaults();
    ToolBar snapshot(registry);
    snapshot.setDock(ToolBar::Dock::Top);
    snapshot.setPalette(defaults.palette.colors, defaults.palette.widths);
    snapshot.setColors(defaults.theme.defaultColor, false);
    snapshot.setWidth(defaults.theme.defaultWidth);
    snapshot.setPaletteOpen(true);
    QImage image(900, 240, QImage::Format_ARGB32_Premultiplied);
    image.fill(defaults.theme.background);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    snapshot.paint(painter, QRectF(0, 0, 900, 240), QRectF(0, 0, 900, 240));
    painter.end();
    image.save(QDir::tempPath() + QStringLiteral("/niricapture-palette.png"));
  }
  if (!check(registry.tools().size() == 12, "Expected 12 built-in tools", error))
    return false;
  if (!check(registry.find(QStringLiteral("select")) != nullptr,
             "Select tool missing", error))
    return false;
  if (!check(registry.defaultToolId() == QStringLiteral("select"),
             "Select must be the default tool", error))
    return false;
  if (!check(registry.find(QStringLiteral("eyedropper")) != nullptr,
             "Eyedropper tool missing", error))
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

  EditorCanvas eyedropperCanvas;
  eyedropperCanvas.setSelection(QRectF(0, 0, 100, 100));
  eyedropperCanvas.setColorSampler([](const QPointF &) {
    return QColor(QStringLiteral("#12ab34"));
  });
  eyedropperCanvas.setTool(QStringLiteral("eyedropper"));
  ToolEvent eyedropperEvent;
  eyedropperEvent.pos = {50, 50};
  eyedropperCanvas.press(eyedropperEvent);
  if (!check(eyedropperCanvas.style().color == QColor(QStringLiteral("#12ab34")),
             "Eyedropper did not update the active colour", error))
    return false;

  {
    EditorCanvas resizeCanvas;
    resizeCanvas.setSelection(QRectF(0, 0, 500, 500));
    resizeCanvas.setTool(QStringLiteral("rectangle"));
    ToolEvent draw;
    draw.pos = {100, 100};
    ToolEvent drawEnd = draw;
    drawEnd.pos = {300, 200};
    resizeCanvas.press(draw);
    resizeCanvas.move(drawEnd);
    resizeCanvas.release(drawEnd);
    resizeCanvas.setTool(QStringLiteral("select"));
    ToolEvent handlePress;
    handlePress.pos = {100, 100};
    if (!check(resizeCanvas.press(handlePress), "Could not grab resize handle",
               error))
      return false;
    ToolEvent shiftMove;
    shiftMove.pos = {150, 150};
    shiftMove.modifiers = Qt::ShiftModifier;
    resizeCanvas.move(shiftMove);
    const Annotation shifted = resizeCanvas.document().annotations().first();
    const qreal shiftedWidth = std::abs(shifted.end.x() - shifted.start.x());
    const qreal shiftedHeight = std::abs(shifted.end.y() - shifted.start.y());
    if (!check(shiftedHeight > 0 &&
                   qAbs(shiftedWidth / shiftedHeight - 2.0) < 0.02,
               "Shift resize did not preserve aspect ratio", error))
      return false;
    resizeCanvas.release(shiftMove);

    EditorCanvas centeredCanvas;
    centeredCanvas.setSelection(QRectF(0, 0, 500, 500));
    centeredCanvas.setTool(QStringLiteral("rectangle"));
    centeredCanvas.press(draw);
    centeredCanvas.move(drawEnd);
    centeredCanvas.release(drawEnd);
    centeredCanvas.setTool(QStringLiteral("select"));
    centeredCanvas.press(handlePress);
    ToolEvent centeredMove;
    centeredMove.pos = {80, 90};
    centeredMove.modifiers = Qt::ControlModifier;
    centeredCanvas.move(centeredMove);
    const Annotation centered =
        centeredCanvas.document().annotations().first();
    const QPointF center = (centered.start + centered.end) / 2.0;
    if (!check(QLineF(center, QPointF(200, 150)).length() < 0.1,
               "Ctrl resize did not keep the centre", error))
      return false;
  }

  const QFont customFont =
      annotationTextFont(5.0, QStringLiteral("DejaVu Sans"));
  if (!check(customFont.family().contains(QStringLiteral("DejaVu")),
             "Custom text font was not applied", error))
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
  bool customColor = false;
  for (const ToolButton &button : paletteBar.buttons(viewport, selection)) {
    if (button.id == QStringLiteral("color-0"))
      colorSelected = button.selected;
    if (button.id == QStringLiteral("width"))
      widthSelected = button.selected;
    if (button.id == QStringLiteral("custom-color"))
      customColor = button.swatch == QColor(QStringLiteral("#00ff00"));
  }
  if (!check(colorSelected, "Colour selection is not marked", error))
    return false;
  if (!check(widthSelected, "Width control is not marked", error))
    return false;
  if (!check(customColor, "Custom colour swatch is missing", error))
    return false;
  if (!check(paletteBar.nextWidth(3) == 9 && paletteBar.nextWidth(9) == 3,
             "Width control does not cycle", error))
    return false;
  const QRectF barRect = paletteBar.rect(viewport, selection);
  if (!check(paletteBar.contains(viewport, selection,
                                 QPointF(barRect.left() + 2,
                                         barRect.center().y())),
             "Toolbar gaps are not part of the hand-cursor area", error))
    return false;
  ToolBar dockBar(registry);
  dockBar.setDock(ToolBar::Dock::Top);
  const QRectF docked = dockBar.rect(viewport, selection);
  if (!check(qFuzzyCompare(docked.top(), viewport.top() + 32.0),
             "Docked toolbar is not on the top border", error))
    return false;

  CaptureData sourceCapture;
  sourceCapture.source = QImage(160, 120, QImage::Format_ARGB32_Premultiplied);
  sourceCapture.source.fill(QColor(QStringLiteral("#ffffff")));
  for (int x = 0; x < sourceCapture.source.width(); x += 8) {
    for (int y = 0; y < sourceCapture.source.height(); y += 8) {
      if (((x / 8) + (y / 8)) % 2 == 0) {
        QPainter stripe(&sourceCapture.source);
        stripe.fillRect(QRect(x, y, 4, 4), QColor(QStringLiteral("#202020")));
      }
    }
  }
  sourceCapture.previewSize = sourceCapture.source.size();
  sourceCapture.monitor.geometry = QRect(QPoint(0, 0), sourceCapture.source.size());
  sourceCapture.monitor.pixelSize = sourceCapture.source.size();
  sourceCapture.monitor.scale = 1.0;

  const auto effectChanged = [&](const QString &toolId) {
    EditorCanvas effectCanvas;
    effectCanvas.setSelection(QRectF(QPointF(0, 0), QSizeF(160, 120)));
    effectCanvas.setTool(toolId);
    ToolEvent effectPress;
    effectPress.pos = {20, 20};
    ToolEvent effectMove = effectPress;
    effectMove.pos = {130, 100};
    effectCanvas.press(effectPress);
    effectCanvas.move(effectMove);
    effectCanvas.release(effectMove);
    if (effectCanvas.document().annotations().size() != 1)
      return false;
    const QImage output = renderCapture(
        sourceCapture, QRectF(0, 0, 160, 120),
        effectCanvas.document().annotations(), BackgroundStyle::None);
    return !output.isNull() && output != sourceCapture.source;
  };
  if (!check(effectChanged(QStringLiteral("mosaic")),
             "Mosaic render did not change pixels", error))
    return false;
  if (!check(effectChanged(QStringLiteral("blur")),
             "Blur render did not change pixels", error))
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
