#include "tool-smoke.hpp"

#include "editor/annotation-document.hpp"
#include "editor/editor-canvas.hpp"
#include "editor/tool-bar.hpp"
#include "editor/tool-registry.hpp"
#include "editor/snap.hpp"
#include "editor/tools/builtin-tools.hpp"
#include "capture.hpp"
#include "config.hpp"

#include <QDir>

#include <QImage>
#include <QKeyEvent>
#include <QPainter>
#include <QWheelEvent>
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

  {
    EditorCanvas shiftCanvas;
    shiftCanvas.setSelection(QRectF(0, 0, 500, 500));
    shiftCanvas.setTool(QStringLiteral("rectangle"));
    ToolEvent createStart;
    createStart.pos = {100, 100};
    ToolEvent createDrag = createStart;
    createDrag.pos = {220, 160};
    createDrag.modifiers = Qt::ShiftModifier;
    shiftCanvas.press(createStart);
    shiftCanvas.move(createDrag);
    const auto square = shiftCanvas.preview();
    if (!check(square.has_value() &&
                   qAbs(std::abs(square->end.x() - square->start.x()) -
                        std::abs(square->end.y() - square->start.y())) < 0.01,
               "Shift creation did not produce a square", error))
      return false;
    shiftCanvas.release(createDrag);
    const Annotation committed =
        shiftCanvas.document().annotations().first();
    if (!check(qAbs(std::abs(committed.end.x() - committed.start.x()) -
                    std::abs(committed.end.y() - committed.start.y())) < 0.01,
               "Shift creation was not committed as a square", error))
      return false;
  }

  {
    EditorCanvas centreCanvas;
    centreCanvas.setSelection(QRectF(0, 0, 500, 500));
    centreCanvas.setTool(QStringLiteral("ellipse"));
    ToolEvent centreStart;
    centreStart.pos = {200, 200};
    ToolEvent centreDrag = centreStart;
    centreDrag.pos = {280, 240};
    centreDrag.modifiers = Qt::ControlModifier;
    centreCanvas.press(centreStart);
    centreCanvas.move(centreDrag);
    const auto centred = centreCanvas.preview();
    if (!check(centred.has_value() &&
                   QLineF((centred->start + centred->end) / 2.0,
                          QPointF(200, 200))
                           .length() < 0.01,
               "Ctrl creation did not keep the centre", error))
      return false;
    centreCanvas.release(centreDrag);
    const Annotation committed =
        centreCanvas.document().annotations().first();
    if (!check(QLineF((committed.start + committed.end) / 2.0,
                      QPointF(200, 200))
                       .length() < 0.01,
               "Ctrl creation was not committed around the centre", error))
      return false;
  }

  {
    EditorCanvas lineCanvas;
    lineCanvas.setSelection(QRectF(0, 0, 500, 500));
    lineCanvas.setTool(QStringLiteral("line"));
    ToolEvent lineStart;
    lineStart.pos = {100, 100};
    ToolEvent lineDrag = lineStart;
    lineDrag.pos = {260, 130};
    lineDrag.modifiers = Qt::ShiftModifier;
    lineCanvas.press(lineStart);
    lineCanvas.move(lineDrag);
    const auto snappedLine = lineCanvas.preview();
    if (!check(snappedLine.has_value() &&
                   qAbs(snappedLine->end.y() - snappedLine->start.y()) < 0.01,
               "Shift creation did not snap the line to 45 degrees", error))
      return false;
    lineCanvas.release(lineDrag);
  }

  {
    EditorCanvas wheelCanvas;
    wheelCanvas.setWidthOptions({2, 4, 8, 12});
    wheelCanvas.style().width = 4;
    QWheelEvent wheelUp(QPointF(50, 50), QPointF(50, 50), QPoint(),
                        QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                        Qt::NoScrollPhase, false);
    if (!check(wheelCanvas.wheel(&wheelUp) &&
                   wheelCanvas.style().width == 8,
               "Wheel did not advance the line width", error))
      return false;
    QWheelEvent wheelDown(QPointF(50, 50), QPointF(50, 50), QPoint(),
                          QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
    if (!check(wheelCanvas.wheel(&wheelDown) &&
                   wheelCanvas.style().width == 4,
               "Wheel did not reduce the line width", error))
      return false;

    EditorCanvas dashedCanvas;
    dashedCanvas.setSelection(QRectF(0, 0, 200, 200));
    dashedCanvas.style().strokeStyle = StrokeStyle::Dashed;
    dashedCanvas.setTool(QStringLiteral("rectangle"));
    ToolEvent dashStart;
    dashStart.pos = {20, 20};
    ToolEvent dashEnd = dashStart;
    dashEnd.pos = {100, 100};
    dashedCanvas.press(dashStart);
    dashedCanvas.move(dashEnd);
    dashedCanvas.release(dashEnd);
    if (!check(!dashedCanvas.document().annotations().isEmpty() &&
                   dashedCanvas.document().annotations().first().strokeStyle ==
                       StrokeStyle::Dashed,
               "Line style was not copied to the annotation", error))
      return false;
  }

  {
    const SnapResult edge =
        snapPointToCanvas(QPointF(498, 300), nullptr, false,
                          QRectF(0, 0, 500, 500), {}, -1);
    if (!check(edge.point.x() == 500 && !edge.guides.isEmpty(),
               "Edge snapping did not produce a guide", error))
      return false;
    const SnapResult diagonal =
        snapPointToCanvas(QPointF(200, 195), new QPointF(100, 100), true,
                          QRectF(0, 0, 500, 500), {}, -1);
    if (!check(qAbs(diagonal.point.x() - 100 - (diagonal.point.y() - 100)) <
                   0.01,
               "45-degree snapping did not constrain the line", error))
      return false;

    EditorCanvas snappedLine;
    snappedLine.setSelection(QRectF(0, 0, 500, 500));
    snappedLine.setTool(QStringLiteral("line"));
    ToolEvent lineStart;
    lineStart.pos = {100, 100};
    ToolEvent lineEnd = lineStart;
    lineEnd.pos = {300, 104};
    snappedLine.press(lineStart);
    snappedLine.move(lineEnd);
    snappedLine.release(lineEnd);
    if (!check(!snappedLine.document().annotations().isEmpty(),
               "Snapped line was not created", error))
      return false;
    const Annotation line = snappedLine.document().annotations().first();
    if (!check(qAbs(line.start.y() - line.end.y()) < 0.01,
               "Line endpoint did not magnetise horizontally", error))
      return false;

    QKeyEvent altOff(QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
    if (!check(snappedLine.keyPress(&altOff) && !snappedLine.snapEnabled(),
               "Alt did not disable snapping", error))
      return false;
    EditorCanvas freeLine;
    freeLine.setSelection(QRectF(0, 0, 500, 500));
    freeLine.setTool(QStringLiteral("line"));
    if (!check(freeLine.keyPress(&altOff), "Alt toggle failed on new canvas",
               error))
      return false;
    freeLine.press(lineStart);
    freeLine.move(lineEnd);
    freeLine.release(lineEnd);
    const Annotation unsnapped = freeLine.document().annotations().first();
    if (!check(qAbs(unsnapped.start.y() - unsnapped.end.y()) > 1.0,
               "Alt-disabled line was still snapped", error))
      return false;
    QKeyEvent altOn(QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
    if (!check(freeLine.keyPress(&altOn) && freeLine.snapEnabled(),
               "Alt did not re-enable snapping", error))
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
    if (button.id == QStringLiteral("size-9"))
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
  bool lineStylePresent = false;
  for (const ToolButton &button : paletteBar.buttons(viewport, selection)) {
    if (button.id == QStringLiteral("line-style"))
      lineStylePresent = true;
  }
  if (!check(lineStylePresent, "Line-style control missing", error))
    return false;
  paletteBar.setStrokeStyle(StrokeStyle::Dashed);
  if (!check(paletteBar.strokeStyle() == StrokeStyle::Dashed,
             "Stroke style was not applied", error))
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
