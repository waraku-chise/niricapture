# Editor architecture

This document covers the modular editor used by region/fullscreen, window and
file editing. The in-place canvas was introduced in 0.3.0 and modularized in
0.4.0; 0.5.0 adds the standalone `EditorWindow` New/Open application using the
same modules.

## Before

`src/region-selector.cpp` was one 1 679-line class. It owned:

- the per-output layer surfaces;
- selection creation, movement and eight resize handles;
- the `Tool` enum and the current tool;
- toolbar buttons, icons, layout, hit-testing and the colour panel;
- per-tool mouse branches for pen/highlighter/arrow/line/rectangle/ellipse/
  text/mosaic/blur/marker;
- annotation bounds/hit-testing/endpoint resizing;
- annotation and source-effect painting;
- `QVector<Annotation>` plus two undo stacks;
- text input and OCR panel;
- composition, clipboard and file export.

That made every new tool touch the canvas class, and temporary tool state
(`freehandPoints_`, drag annotations) lived beside permanent document state.

`src/editor.cpp` (3 202 lines) is the older `CaptureEditor`. It is no longer
used by production CLI modes; window and file inputs now open
`src/editor-window.cpp`. The old class is retained only for its large
regression suite while the last standalone-only features are ported.

## Target architecture

```
src/editor/
├── tool.hpp                    IEditorTool, ToolContext, ToolEvent, EditorStyle
├── config.hpp/cpp              JSON config: tools, shortcuts, palette, canvas, theme
├── annotation-document.*       annotations + selection + one undo/redo history
├── annotation-geometry.*       bounds, hit tests, endpoint handles, translation
├── tool-registry.*             owns tool instances, lookup by stable id
├── tool-bar.*                  ToolButton model, layout, painting, hit testing
├── editor-canvas.*             event dispatch, current tool, preview, vector paint
├── editor-window.*             standalone New/Open application window
└── tools/
    ├── builtin-tools.*         the only place that registers built-in tools
    ├── select-tool.*
    ├── two-point-tool.*        shared drag-created shape base
    ├── arrow-tool.*
    ├── line-tool.*
    ├── rectangle-tool.*
    ├── ellipse-tool.*
    ├── freehand-tool.*         shared pen/highlighter base
    ├── pen-tool.*
    ├── highlighter-tool.*
    ├── text-tool.*
    ├── mosaic-tool.*
    ├── blur-tool.*
    └── marker-tool.*
```

`RegionSelector` is now the surface/selection/export adapter:

```
layer input
   │
   ▼
RegionSelector ── selection handles / new selection / OCR / export
   │  global ToolEvent
   ▼
EditorCanvas ── current IEditorTool ── ToolContext ── AnnotationDocument
   │                                      │
   │                                      ├─ setStatus
   │                                      ├─ requestRepaint
   │                                      └─ beginText
   ▼
paintVectors / preview
   │
   ▼
RegionOverlay paints frozen frame + source effects + vectors + chrome
ToolBar paints buttons from ToolRegistry metadata
```

### IEditorTool

`src/editor/tool.hpp`:

```cpp
class IEditorTool {
  virtual QString id() const;
  virtual QString name() const;
  virtual QString icon() const;
  virtual QString shortcut() const;
  virtual QString tooltip() const;
  virtual ToolGroup group() const;

  virtual void activate(ToolContext &);
  virtual void deactivate(ToolContext &);
  virtual void reset() = 0;

  virtual bool mousePress(const ToolEvent &, ToolContext &);
  virtual bool mouseMove(const ToolEvent &, ToolContext &);
  virtual bool mouseRelease(const ToolEvent &, ToolContext &);
  virtual bool keyPress(QKeyEvent *, ToolContext &);
  virtual bool wheel(QWheelEvent *, ToolContext &);

  virtual std::optional<Annotation> preview() const;
  virtual void paintPreview(QPainter &, const ToolContext &) const;
};
```

`preview()` returns a normal `Annotation`, not a separate preview type. That
keeps undo, vector painting and export on one data model. `reset()` must clear
all temporary state; `EditorCanvas::setTool()` calls it on both the old and new
tool, so a switch cannot leave a stale preview or mouse state.

`ToolContext` is a short-lived service object: document, selection, style and
three callbacks. Tools never retain it and never call each other.

### AnnotationDocument

One model for annotations, selected index, marker counter, undo and redo.
`pushUndo()` records a snapshot; tools call it only immediately before a
committed mutation. Edit-state snapshots include selection and the marker
counter, so undo restores numbering and selection as well as geometry.

### AnnotationGeometry

Shared bounds, hit-testing, endpoint handles and translation. The canvas and
tools use the same geometry, so a tool cannot disagree with the renderer about
where an annotation is.

### ToolRegistry

Owns `std::unique_ptr<IEditorTool>` instances and returns them by stable id.
`registerBuiltinTools()` is the single registration point. Adding a tool means:

1. add a tool class and source file;
2. register it in `builtin-tools.cpp`;
3. compile.

No switch in `EditorCanvas` or `ToolBar` is required.

`ToolRegistry::setVisibleTools()` and `ToolBar::setCommands()` / `setPalette()`
are driven by `config.json`; `EditorCanvas::setToolShortcuts()` and
`setDefaultStyle()` consume the same file. See `docs/CONFIGURATION.md`.

### ToolBar / ToolButton

The toolbar is a pure model/painter. It receives a registry and the current
selection/viewport, builds `ToolButton` records (active/hovered/pressed/
enabled), paints them and returns a `ToolBarHit` (`isTool` + id). It does not
know what a pen or mosaic does.

Commands (`palette`, `undo`, `redo`, `ocr`, `copy`, `save`, `close`) are
toolbar ids handled by `RegionSelector`; tool ids are passed to
`EditorCanvas::setTool()`.

### EditorCanvas

Owns the registry, document, style, selection and current tool. It translates
a global logical event into `ToolEvent`, dispatches to the current tool,
maintains the uncommitted preview, and paints the vector layer. It does not
know about layer surfaces, screens, output frames, OCR or output files.

## Migration map

| Old `RegionSelector` | New owner |
|---|---|
| `Tool` enum and `tool_` | `ToolRegistry` + `EditorCanvas::currentToolId` |
| `toolbarRect/popupRect/toolbarButtons/toolbarHit/paintToolbar` | `ToolBar` |
| `Button` struct and toolbar icon switch | `ToolButton` + `ToolBar` |
| `handleAction()` | `RegionSelector::handleToolbarHit` (commands) / `EditorCanvas::setTool` |
| `previewAnnotation()` | each tool's `preview()` |
| pen/highlighter branches in `editorPress/Move/Release` | `PenTool` / `HighlighterTool` |
| arrow/line/rectangle/ellipse branches | `ArrowTool` / `LineTool` / `RectangleTool` / `EllipseTool` |
| text branch + `beginText` | `TextTool` + `RegionSelector::beginText` |
| marker branch | `MarkerTool` |
| mosaic/blur branches | `MosaicTool` / `BlurTool` |
| annotation selection/move/endpoint resize | `SelectTool` |
| `annotationAt/annotationBounds/annotationHandleAt` | `annotation-geometry.*` |
| `annotations_`, `undo_`, `redo_`, `nextMarker_` | `AnnotationDocument` |
| annotation and preview painting | `EditorCanvas::paintVectors` |
| source-pixel effects preview | `RegionSelector::paintSourceEffects` (per output frame) |
| selection creation/move/resize | `RegionSelector` (not an annotation tool) |
| text widget / OCR panel / export | `RegionSelector` (host services) |

## Undo and rendering

- Tools commit exactly one annotation on release.
- `pushUndo()` is called immediately before the first document mutation.
- `SelectTool` snapshots before move/resize; `wheel` snapshots before scaling.
- Undo/redo is one stack shared by all tools; there is no per-tool history.
- Vector annotations and previews are painted by `EditorCanvas`.
- Mosaic/blur preview and final export still sample the original frozen image
  in `RegionSelector::paintSourceEffects` and `renderCapture` respectively.
  The export path uses `applyRedactionsScaled` before vector painting, so the
  source pixels are never left recoverable under a vector scribble.

## Tests

- `tests/tool-smoke.cpp` checks registry size/lookup, document undo/redo,
  every draw tool, marker numbering, select/move, tool-switch cleanup, text
  callback, toolbar hit testing and palette records.
- `tests/editor-smoke.cpp` retains the standalone editor regression suite.
- `make check` runs the contracts, smoke suite and lint.
- Live hooks:
  - `NIRICAPTURE_TEST_SELECT=...` drives a selection;
  - `NIRICAPTURE_TEST_GEOMETRY=1` creates then expands a selection;
  - `NIRICAPTURE_TEST_DRAW=1` draws a rectangle/arrow/marker/mosaic;
  - `NIRICAPTURE_TEST_OCR=1` opens the real OCR panel;
  - `NIRICAPTURE_TEST_EXPORT=1` writes the composed result.

## Scope and remaining work

The modular canvas is used by:

- region/fullscreen in `RegionSelector`;
- window capture and image/file editing in `EditorWindow`;
- the New/Open launcher entry.

`src/editor.cpp` remains compiled only for the regression suite. Its
standalone-only features have not all been ported to `EditorWindow` yet:

- Spotlight
- Eyedropper
- Cutout
- the old shape/colour popup layout
- the old multi-select/crop interaction model

The new editor intentionally uses the modular toolbar/canvas, a New/Open home
page and explicit canvas adjustment instead of the old in-window flow. The
next step is to add `SpotlightTool`, `EyedropperTool` and a cutout command to the
new canvas, then remove `CaptureEditor` once its regression suite is migrated.
