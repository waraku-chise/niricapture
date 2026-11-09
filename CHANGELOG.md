# Changelog

## [0.6.4] — 2026-11-09

- The standalone editor start page no longer uses a card or displays the
  application name. It shows only the New canvas / Open image actions, with a
  Quit button at the bottom.
- Window title is now “Editor” / “编辑器” rather than the product name.

## [0.6.3] — 2026-11-09

### Editor fixes

- The standalone editor's annotation toolbar is now docked to the top border
  instead of floating over the canvas; the image area is laid out below it.
- Mosaic and Gaussian blur now render a live preview during drawing and after
  commit, before the vector layer. Export behavior is unchanged and verified
  against `renderCapture`.
- Tests cover the docked toolbar geometry and that mosaic/blur actually change
  rendered pixels.

## [Unreleased]

### Configuration v2

- `AppConfig::toJson()` / `toJsonBytes()` serialize the full configuration.
- `save()` / `saveTo()` write through `QSaveFile` (atomic replace).
- Unknown JSON keys are preserved on save.
- Schema `version: 2` with in-memory migration from v1 and rejection of newer
  schemas (including refusing to overwrite them).
- `--print-config` prints the normalized v2 JSON.
- `tests/config-smoke.cpp` covers roundtrip, unknown-field preservation,
  migration, newer-schema rejection and atomic save/reload.

## [0.6.2] — 2026-11-06

- The pointing-hand cursor now covers the whole toolbar panel and palette,
  including gaps and separators, not only button hit areas.

## [0.6.1] — 2026-11-06

- Selected palette colour now gets a visible accent ring.
- Line-width options are drawn as circles whose size reflects the width, with
  the active width ringed.
- The pointer becomes a pointing hand over the toolbar (including the palette)
  instead of the crosshair used for drawing.
- Added toolbar selection assertions to `tests/tool-smoke.cpp`.

## [0.6.0] — 2026-11-06

### Config-driven editor

- Added `~/.config/niricapture/config.json` (auto-created, documented in
  `docs/CONFIGURATION.md`).
- Toolbar tool order/visibility, command order, tool shortcuts, palette colors
  and widths, default style, canvas size/padding/background, theme colors, OCR
  languages and screenshot directory are configurable.
- `--config PATH`, `NIRICAPTURE_CONFIG` and `--print-config` are supported.
- Missing keys inherit defaults; malformed JSON falls back to defaults without
  overwriting the user file.
- Added `tests/config-smoke.cpp` for parsing and applying config.
- Niri keybindings remain in the Niri config; window capture is `Ctrl+F1`.

## [0.5.0] — 2026-11-06

### Standalone editor

- `niricapture --editor` opens a normal application window with a New/Open
  start page.
- The launcher entry `NiriCapture Editor` uses that mode; screenshot capture
  remains available from the existing Niri binds.
- New canvas dialog, image open, save, copy, pin, OCR and zoom/fit controls.
- Canvas adjustment dialog: width, height, padding and background
  (transparent, white, dark, Aurora, Sunset, Lagoon, Violet).
- Zoom with Ctrl+wheel, pan with middle-drag or Space+drag.
- Window capture now opens the same modular editor window; the old standalone
  `CaptureEditor` path is no longer used by production CLI modes.
- Window capture bind moved to **Ctrl+F1**.

## [0.4.0] — 2026-11-06

### Modular editor tools

- Split the in-place editor into `IEditorTool`, `ToolRegistry`, `ToolBar`,
  `EditorCanvas`, `AnnotationDocument` and `annotation-geometry` modules under
  `src/editor/`.
- Each built-in tool (select, pen, arrow, line, rectangle, ellipse, text,
  highlighter, mosaic, blur, numbered marker) has its own module and owns its
  temporary interaction state.
- Toolbar layout, buttons, palette and hit testing no longer know how any tool
  draws.
- Undo/redo, marker numbering, hit testing and vector rendering now use one
  shared data model. Mosaic/blur continue to sample the original frozen image
  on export.
- Added `tests/tool-smoke.cpp` for registry, every draw tool, tool switching,
  text callback, select/move and toolbar/palette hit testing.
- The standalone window/file editor keeps its legacy internals for now to avoid
  regressing its extra tools; see `docs/EDITOR-ARCHITECTURE.md`.

## [0.3.0] — 2026-11-05

### True in-place toolbar

- Region and current-output fullscreen capture now use a real toolbar painted
  directly in the frozen selection overlay. The previous embedded `CaptureEditor`
  child was removed from the region path.
- The selection is not a fixed crop: its eight handles can enlarge it as well as
  shrink it, using pixels from the original full frozen output.
- Annotations are stored in global logical coordinates, painted across outputs
  and translated into the current selection only at export.
- Tools: select, pen, arrow, line, rectangle, ellipse, text, highlighter,
  mosaic, blur, numbered marker; undo/redo; colour, width and fill; OCR;
  copy/save/close.
- Window, image-file and clipboard-image editing still use the standalone
  editor.

### Chinese interface

- UI language follows the system locale; `NIRICAPTURE_LANG=zh_CN` or `en`
  overrides it.
- Selection hints, toolbar status, OCR panel and common editor messages are
  localized.

## [0.2.0] — 2026-11-05

### Editing in the frozen selection

- Region and current-output fullscreen captures now embed the editor directly in
  the frozen selection overlay. The toolbar appears next to the selection and
  there is no second editor window or image jump.
- The in-place editor keeps the captured pixels at their original screen
  coordinates; the area outside the selection remains the dimmed frozen frame.
- Window, image-file and clipboard-image editing still use the standalone
  editor.
- `Q` toggles the colour / line-width panel from the keyboard.

### Chinese interface

- UI language follows the system locale. Chinese is selected for
  `zh_CN`/Chinese locales; `NIRICAPTURE_LANG=zh_CN` or `en` overrides it.
- Tooltips, status bar, OCR panel and selection hints are localized.

## [0.1.0] — 2026-11-05

First Niri/DMS-focused release.

### Capture

- Region and fullscreen capture freeze every output through
  `dms screenshot output --stdout` before mapping any overlay.
- Multi-output logical desktop model with per-output native pixels and a
  documented target-scale composition rule.
- Niri-native window capture through
  `niri msg action screenshot-window --id`, including occluded and
  off-workspace windows. No desktop crop fallback.
- `--list-windows`, `--window-id`, `--capture-all`, `--output`.
- Single-instance hand-off and stale-lock recovery.

### Editor

- Standalone Qt 6 layer-shell editor independent of the DMS shell lifecycle.
- Compact floating toolbar with shape/colour/width popups.
- Select/move/resize, pen, arrow, line, outlined/filled rectangle,
  outlined/filled ellipse, text, highlighter, numbered marker, mosaic and
  real Gaussian blur.
- Non-destructive annotation model with undo/redo, deletion, colour and width.
- Copy/save/open, image pinning, file and clipboard image input.

### OCR

- Optional Tesseract OCR on a worker thread.
- Selectable result panel instead of an automatic clipboard-only result.
- Automatic `eng+chi_sim` when the Chinese model is installed; environment
  override.

### Fixed

- Region capture no longer jumps to the centre of the editor. The captured
  image stays at its original logical screen position, so its pixels line up
  with the dimmed desktop behind it. Single-output fullscreen captures are
  also presented at 1:1 instead of being shrunken and centred.

### Packaging

- Arch CMake build, local installer/uninstaller, PKGBUILD and Niri bindings.
- MIT attribution for the Nirisnap-derived native engine.
- Research and test records in `docs/`.

### Known gaps

- No result thumbnail popup.
- No scroll capture, GIF or video recording.
- No portal/PipeWire fallback.
- Live multi-monitor testing was not possible on the development machine;
  composition is covered by synthetic tests.
