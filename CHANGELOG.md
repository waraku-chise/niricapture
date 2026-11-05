# Changelog

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
