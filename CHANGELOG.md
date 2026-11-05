# Changelog

## [0.2.0] — 2026-11-05

### Editing in the frozen selection

- Region and current-output fullscreen captures now embed the editor directly in
  the frozen selection overlay. The toolbar appears next to the selection and
  there is no second editor window or image jump.
- The embedded editor keeps the captured pixels at their original screen
  coordinates; the area outside the selection remains the dimmed frozen frame.
- Window, image-file and clipboard-image editing still use the standalone
  editor.
- `Q` / `G` toggle the colour and shape panels from the keyboard.

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
