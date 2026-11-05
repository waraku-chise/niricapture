# Source research and architecture decisions

This document records what was actually read/executed before implementation.
It deliberately separates verified facts from design choices.

## 1. DMS Screenshot+ (MIT)

Repository: <https://github.com/pcmid/dms-screenshot-plus>
Licence: MIT, Copyright (c) 2026 pcmid.

### What it actually does

- It is a **DMS/Quickshell plugin**, not a standalone application.
- `ScreenshotPlusDaemon.qml` collects screens from `Quickshell.screens`, then
  runs, for each output:

  ```text
  dms screenshot output -o NAME --no-clipboard --no-notify
      --dir $XDG_RUNTIME_DIR/screenshot-plus --filename freeze-NAME.png
      --format png --json
  ```

  It starts every grab before mapping the overlay. The overlay is transparent
  until the PNGs return. `CaptureOverlay.qml` then draws `Image` items with the
  frozen PNGs; it does not recapture while selecting or annotating.
- The alternate `screencopy` backend uses Quickshell `ScreencopyView` with
  `live: false`. The project's README documents that stock Quickshell 0.3.1
  crashes on overlay close due to a second `wl_output` listener
  ([quickshell#1094](https://github.com/quickshell-mirror/quickshell/issues/1094)).
  NiriCapture does not use that backend.
- One `PanelWindow` per screen, `WlrLayer.Overlay`, anchors on all edges,
  `exclusiveZone: -1`, keyboard focus exclusive on the selection owner.
- Selection/strokes are stored in **global logical coordinates**. Each overlay
  subtracts its screen origin. Export uses `grabToImage` at the native resolution
  implied by the output scale.
- `dms screenshot output --json` returns a JSON object with `status: success`
  when a file is written. With `--stdout` and without `--json`, stdout is raw
  PNG bytes. Both behaviours were verified against the installed DMS 2.0.
- Annotation data is an array of plain strokes:
  `{id, tool, color, width, points[], text, w, h, ...}`. Undo history snapshots
  the whole array. Mosaic is painted by `ShaderEffectSource` at source pixels,
  not by a translucent rectangle.
- OCR invokes `oar ocr --format json` (PaddleOCR models). It is not Tesseract.
- Pinned images use separate layer windows.

### What NiriCapture reuses

- The exact `dms screenshot output` freeze strategy (per output, `--stdout`,
  no clipboard/file side effects) and the "capture before mapping" ordering.
- The coordinate rule: store global logical coordinates; subtract output
  origin for painting.
- The non-destructive stroke model and source-pixel redaction requirement.
- The failure policy: if a frame cannot be grabbed, do not map an overlay and
  do not silently fall back to a live/partial desktop.

### What NiriCapture does not reuse

- Quickshell/QML and the DMS plugin lifecycle. A separate native Qt process is
  required so the editor survives a DMS shell restart and so DMS's own popouts
  and layer surfaces never appear in the capture.
- `ScreencopyView` (crash risk on stock Quickshell).
- oar/PaddleOCR as a hard dependency. NiriCapture uses optional Tesseract.

DMS Screenshot+ source was read under MIT. NiriCapture contains no verbatim
copy of its QML/JS; the workflow and a few geometric rules were reimplemented
in C++.

## 2. HyprCapture (GPL-3.0)

Repository: <https://github.com/gfhdhytghd/HyprCapture>
Licence: GPL-3.0. It is a Hyprland compositor plugin plus a Qt layer-shell
helper.

### UI and interaction facts

- The helper is a `QMainWindow` with a compact grouped toolbar, popups for
  related tool variants, a colour palette/width control, and a separate
  in-place editor.
- The toolbar follows the screenshot edge, moves above it when there is no room
  below, and clamps to the screen.
- Modes: region, fullscreen, window, fusion. Window selection supports hover,
  wheel cycling and keyboard; a result thumbnail appears at the bottom right
  with open/copy/show/delete/close and swipe gestures.
- Annotation export uses native image coordinates. Window background,
  decorations, shadow and watermark are configurable.
- It captures window artifacts in the compositor plugin, including occluded
  windows, because Hyprland exposes those artifacts to plugins.

### What NiriCapture reuses

- Interaction/layout ideas only: one compact floating surface, grouped tools,
  secondary popups, size/colour persistence, selection handles, keyboard-first
  operation, non-destructive annotation state.
- The principle that direct output and edit mode are separate flows.

### What NiriCapture does not reuse

- No HyprCapture code, no Hyprland headers, no compositor plugin, no recording,
  HDR, scrolling or watermark subsystems.
- Niri has no plugin ABI equivalent. The supported equivalent is Niri's
  documented `screenshot-window` action, which is used directly.

Because HyprCapture is GPL-3.0 and NiriCapture is AGPL-3.0-only, even code
copying would be licence-compatible only under AGPL terms; this project avoids
the question by not copying it.

## 3. Niri 26.04 capabilities (verified in source and live)

Source: <https://github.com/YaLTeR/niri>, tag `v26.04`.

### Window screenshot

`niri msg action screenshot-window --help` on the running 26.04 compositor
shows `--id`, `--path`, `--write-to-disk`, `--show-pointer`.

In `niri-ipc/src/lib.rs`, `Action::ScreenshotWindow` has an `id: Option<u64>`.
`src/input/mod.rs` dispatches `Action::ScreenshotWindowById` by finding the
mapped window and its monitor, then calling `Niri::screenshot_window(...)`.

`src/niri.rs::screenshot_window`:

1. Uses the target output's fractional scale.
2. Calls `mapped.render(...)` into a fresh element list.
3. Computes `encompassing_geo` (including shadow/border), renders to an
   `Abgr8888` texture, and saves it.
4. Applies the window's opacity unless fullscreen/ignore-opacity.
5. Includes the pointer only if requested and currently over that window.

It never samples the composited output. This is why a fully covered window and
a window on another workspace can be captured correctly. Live tests in
`docs/TESTING.md` confirm both.

Limits:

- The action requires a window id. The public `niri msg pick-window` returns
  metadata and is the intended interactive selector.
- If the id does not exist, Niri 26.04 silently does nothing and still exits 0.
  NiriCapture validates the id against the IPC window list and waits for a
  readable PNG.
- `--path` must be absolute; the file is written asynchronously after the CLI
  exits.
- Niri always installs the raw PNG as the clipboard selection in
  `save_screenshot`; there is no opt-out.

### Niri IPC window metadata

`niri msg --json windows` exposes:
`id`, `title`, `app_id`, `pid`, `workspace_id`, `is_focused`, `is_floating`,
`is_urgent`, `layout.pos_in_scrolling_layout`, `layout.tile_size`,
`layout.window_size`, `layout.tile_pos_in_workspace_view`,
`layout.window_offset_in_tile`, and focus timestamp.

It does **not** expose:
- the current scrolling viewport offset,
- an absolute global screen rectangle for each window,
- stacking order across outputs.

Therefore a custom hover overlay that maps every scrolling-layout window to
screen pixels would be guesswork. NiriCapture uses Niri's native picker for
interactive hover/highlight and provides `--list-windows` for automation.

### Other Niri paths considered

- **xdg-desktop-portal + PipeWire window capture**: Niri's screencasting code
  has `CastTarget::Window`, and Niri can expose window casts to portals.
  However this is a negotiated video stream (PipeWire), not a one-shot still
  buffer. It would add portal permissions, stream startup and frame extraction.
- **`screenshot-window` without an id**: captures only the focused window, and
  would require changing focus (visible side effect).
- **Desktop crop**: rejected. It cannot produce an occluded window's full
  content.
- **Niri core patch**: unnecessary; `screenshot-window --id` already provides a
  true independent render.

Niri's own screenshot UI notes that cross-output selections are unsupported
because of mixed scales; NiriCapture handles the multi-output case itself by
compositing per-output native frames at a chosen target scale.

## 4. Coordinate model

- `MonitorInfo.geometry` is the transformed logical rectangle from Niri's
  `outputs` JSON.
- `MonitorInfo.pixelSize = round(logical * scale)`; the actual DMS PNG size is
  read back and used as the authoritative pixel size.
- A selection is one global logical rectangle.
- For each output:
  - `intersection = selection ∩ output.geometry`
  - source rect = `(intersection - output.geometry.topLeft) * outputPixelScale`
  - destination rect = `(intersection - selection.topLeft) * targetScale`
- `targetScale` is the scale of the output covering the largest selection area.
  This preserves native detail for normal single-output selections; a
  cross-output selection is resampled into the dominant output's pixel grid.
- Areas with no output remain transparent.

This is unit-tested with synthetic 1.0 and 1.25 scale outputs.

## 5. Dependency and licence conclusions

- Qt 6 Widgets + LayerShellQt is the smallest stack that can draw per-output
  layer-shell surfaces, keep the capture UI alive independently of DMS, and
  render/export pixels without a compositor plugin.
- Quickshell is not needed at runtime and its stock screencopy backend has a
  known crash; avoiding it is a deliberate reliability choice.
- Tesseract is packaged in Arch and supports offline `eng+chi_sim`; OCR is
  optional and runs in a worker thread. PaddleOCR/oar (used by DMS
  Screenshot+) is more accurate on some CJK layouts but pulls a large
  Paddle runtime or a Rust helper plus model downloads. For an Arch desktop
  tool with an optional dependency, Tesseract gives the best
  size/availability trade-off. The OCR invocation is isolated behind
  `recognizeText()` so a Paddle backend can be added later.
- DMS is invoked via its CLI only. Niri is queried via JSON IPC and its public
  actions only. No core source is modified.
- AGPL-3.0-only for the combined work; MIT notice for the Nirisnap-derived
  engine is preserved.

## 6. Explicit architecture conclusions

### 6.1 Technical architecture of the two projects

| Project | Architecture |
|---|---|
| DMS Screenshot+ | Quickshell plugin. One `PanelWindow` per output; `Proc.runCommand` starts `dms screenshot output` per output; frozen PNGs decoded into `Image`; annotations are QML `Canvas` strokes; export via `grabToImage`; optional in-process `ScreencopyView` backend. |
| HyprCapture | Hyprland plugin + separate Qt `QMainWindow` layer-shell helper. Plugin captures compositor window/desktop artifacts; helper owns selection, in-place annotations, output, thumbnail and recording. GPL-3.0. |

### 6.2 Reused directly (with licence)

| Source | Reuse |
|---|---|
| Nirisnap (MIT) | Native Qt editor/annotation engine, pin windows, instance lock, clipboard/OCR plumbing; attribution retained. |
| DMS Screenshot+ (MIT) | Verified freeze workflow and coordinate rules reimplemented in C++; no QML copied. |
| Niri (GPL-3.0, separate process) | Public IPC/actions only; no linking or source modification. |

### 6.3 Reimplemented for Niri

| Need | Reason |
|---|---|
| Multi-output frozen selection | DMS Screenshot+ is a shell plugin and exports per owner screen; NiriCapture needs a standalone overlay and cross-output composition. |
| Independent window capture | Niri has no Hyprland plugin ABI, but exposes `screenshot-window --id`, which renders the mapped window directly. |
| Native toolbar/editor | Avoid Quickshell screencopy crash and shell lifecycle coupling; use Qt Widgets + LayerShellQt. |
| Composition/scale model | Mixed output scales are not handled by Niri's own cross-output screenshot UI. |

### 6.4 Dependencies

Required: Niri, Qt 6 Widgets, LayerShellQt, `grim`, `wl-clipboard`, `libnotify`.
Recommended: `dms`/`dux-shell` for the freeze backend.
Optional: Tesseract + `eng`/`chi_sim` models.
Explicitly absent: Quickshell, Noctalia, Hyprland headers, compositor plugins,
xdg-desktop-portal/PipeWire, OpenCV, PaddleOCR.

### 6.5 Main risks and mitigations

| Risk | Mitigation |
|---|---|
| DMS CLI output format changes | Prefer PNG stdout; validate decoded image; fall back to `grim`; one output at a time; surface errors. |
| Mixed/fractional scales | Use actual DMS PNG size as authority; compose per-output source rects; synthetic 1.25 tests; document dominant-output scale rule. |
| Window id disappears | Validate id against IPC list; require readable non-empty PNG written after the action. |
| Niri always writes window PNG to clipboard | Documented; no false restoration; annotated copy overwrites it later. |
| DMS shell restart | Editor is a separate process with no DMS imports; frozen frames are already in memory. |
| Overlay input/focus | One layer surface per output; exclusive keyboard on the selection owner; real `wtype` tests for F/Enter/Esc. |
| GPL contamination | No HyprCapture code copied; Niri used only through IPC; AGPL-compatible MIT attribution retained. |

### 6.6 Final architecture

A single unprivileged Qt executable:

1. probes Niri outputs/windows over JSON IPC;
2. freezes outputs through the DMS CLI (region/fullscreen/all), or captures a
   window through Niri's native action;
3. presents per-output layer surfaces and paints the floating toolbar and
   annotations directly on the frozen owner surface;
4. stores annotations non-destructively and renders source effects at export;
5. copies via `wl-copy`, saves via QImage PNG, and can pin the result.

No daemon, no shell plugin, no compositor patch, no portal.
