# Testing record

Environment during live validation:

- Niri **26.04** (`8ed0da4`), single output `DVI-D-1` at 1920×1080, scale 1.0.
- DMS (`dux-shell`) **2.0.0**, Quickshell 0.3.2 installed but unused.
- Qt **6.12.0**, LayerShellQt 6.8.2, Tesseract 5.5.4 with `eng` + `chi_sim`.
- Arch Linux, Wayland session `wayland-1`.

The whole suite is offscreen and does not need a compositor:

```bash
make check
# or
QT_QPA_PLATFORM=offscreen ./build/niricapture-smoke build/niricapture-smoke-output
QT_QPA_PLATFORM=offscreen ./build/niricapture-desktop-smoke
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

Result at the time of writing:

- Qt interaction suite: **0**, all checks passed.
- Composition/effects/OCR suite: **0**.
- Python contracts: **17 tests, all passed**.
- The build uses `-Wall -Wextra -Wpedantic`; `make lint` is a separate,
  slower clang-tidy pass.

## Live DMS frozen-frame test

1. `niricapture --capture-fullscreen --save` → 1920×1080 PNG.
2. `NIRICAPTURE_TEST_SELECT=100,100,700,500 niricapture --capture-region
   --save` opened a real layer-surface overlay and saved 700×500.
3. The region output was compared with the corresponding crop of the
   fullscreen freeze:

```
sizes (1920, 1080) (700, 500) (700, 500)
difference bbox None
max channel difference (0, 0)
mean difference 0.0
```

The selection overlay showed a static frozen image, dimming outside the
selection, eight handles, a `700 × 500 · 700 × 500 px` readout and the
`Enter to edit` hint. This confirms that the eventual export uses the same
frozen pixels as the overlay, not a second capture.

## Live keyboard and clipboard checks

The real layer surfaces were driven with `wtype`:

- Launch region capture, press `F`, press `Enter`: an 1920×1080 PNG was saved.
  This proves the overlay receives exclusive keyboard focus and that `F`
  selects the frozen desktop.
- Launch region capture, press `Escape`: the process exited 0 and wrote no
  file.
- `niricapture --capture-fullscreen --copy` then `wl-paste --type image/png`
  returned a valid 1920×1080 RGBA PNG.

## In-place toolbar

The live flow was verified with the real DMS freeze path:

- `NIRICAPTURE_TEST_SELECT=350,180,1100,700 niricapture --capture-region`
  mapped the frozen overlay and displayed the toolbar directly on the
  selection, without a second window.
- The image area at `(350,180)` was pixel-identical to the frozen frame crop;
  the remainder stayed dimmed.
- Selection handles resize the global selection against the full frozen output,
  so the region can be expanded again after shrinking; there is no crop-only
  limit.
- `Ctrl+C` copied a 1100×700 PNG and exited; `Ctrl+S` saved a 1100×700 PNG.
- `Q` opened the colour/line-width panel with the Chinese status
  `颜色 / 线宽`.

## Standalone editor anchoring

Window and file editing always use the standalone `EditorWindow`; the old
`CaptureEditor` anchoring test was removed with the legacy editor.

## Live Niri native window tests

### Off-workspace window

The existing Edge window (`id 20`) was on workspace 6 while workspace 2 was
active. Capture:

```
niri msg action screenshot-window --id 20 --path /tmp/niri-window-test.png ...
→ 1916 × 1044 RGBA PNG
```

The larger-than-content size is Niri's window shadow/border envelope; alpha is
preserved. This is Niri's native render, not a desktop crop.

### Partially and fully occluded window

Two XTerm windows were floated on the active workspace:

- target `id 33`, 600×400 at logical (128,160), dark blue background with
  `NIRICAPTURE TARGET WINDOW 12345`;
- cover `id 34`, resized to 600×400 and then **fullscreened** (1920×1080), red
  background with `NIRICAPTURE COVER WINDOW`.

Captures of target `id 33` were taken before and after the cover became
fullscreen:

```
partial: screenshot-...png 600×400 RGBA
full:    screenshot-...png 600×400 RGBA
identical: True
center pixel: (16, 32, 48, 255)
tesseract: "NIRICAPTURE THRGET WINDOW 12345"
           "aeclusion test payload"
```

The OCR typo comes from the test image font, not from the capture path. The red
cover window never appears in either capture. This satisfies the revised
requirement: a completely covered target still yields its complete own image.

### Transparent window

A Ghostty window with `background-opacity=0.82` was floated and captured:

```
700 × 500 RGBA
alpha min/max: 209 / 255
corner/background pixel: (13, 21, 28, 209)
```

Niri's independent render preserves window alpha. Rounded-corner clipping is
produced by Niri/window rules when configured; NiriCapture does not flatten or
re-composite the window onto the desktop.

## OCR

The offscreen suite renders a clean card containing:

```
你好，世界
Hello screenshot 42
```

and calls `recognizeText` with `NIRICAPTURE_OCR_LANGS=eng+chi_sim`. Both a
Chinese word and the English line are found. The test is skipped if Tesseract
or `chi_sim.traineddata` is absent.

The interaction suite OCRs the standalone editor and verifies the selectable
result panel; `Esc` closes the panel before the editor.

## Multi-output composition

Live hardware had only one output, so multi-output behaviour is covered by
synthetic tests in `tests/desktop-capture-smoke.cpp`:

- left output: 800×600 logical/native;
- right output: 640×480 logical at 1.25 scale, 800×600 native;
- a 400×200 selection crossing the boundary composes to 500×250 at target
  scale 1.25, sampling each output from its own frame;
- "all outputs" composes to 1800×750 and leaves the logical gap below the
  shorter right output transparent.

Coordinate math is deterministic and independent of hardware, but a second
physical monitor was not available for an end-to-end live run. This is stated
in the README as a tested limitation.

## Window-list contract

`--list-windows` was exercised against a synthetic `niri` executable:

```json
[{"id":"7","title":"Editor","app_id":"dev.editor"}]
```

A failing `niri` process returns exit code 1 and a visible error message.

## Installer and packaging

- `packaging/install-local.sh --check` performs no writes and does not require
  sudo.
- The installer refuses to overwrite an existing binary.
- `packaging/uninstall-local.sh --check` lists only known install paths and
  refuses `/`, `/usr`, and `$HOME` as prefixes.
- CMake install is staged with `cmake --install build --prefix "$PWD/stage/usr"`
  in CI.
- No Niri/DMS config is read or written by install/uninstall.

## Not tested / not implemented

- A second physical monitor and mixed-DPI live capture (synthetic coverage
  only).
- Scroll capture, GIF/video recording.
- Result thumbnail popup.
- A custom absolute-position window hover map across Niri's scrolling layout
  (Niri IPC lacks the viewport offset).
- PipeWire/portal window capture. The native action is the supported path.

These are recorded instead of being claimed as complete.

## Modular editor checks

`niricapture-smoke` now includes `tests/tool-smoke.cpp`, which verifies:

- all 11 built-in tools register exactly once;
- document undo/redo and marker numbering;
- every draw tool commits the expected `Annotation::Kind`;
- select tool hit/move;
- switching tools drops the previous preview without mutating the document;
- text tool requests the host editor;
- toolbar buttons and palette hit-testing.

Live integration hooks used during this pass:

```bash
NIRICAPTURE_TEST_SELECT=350,180,1100,700 NIRICAPTURE_TEST_GEOMETRY=1 \
  niricapture --capture-region
NIRICAPTURE_TEST_SELECT=350,180,1100,700 NIRICAPTURE_TEST_DRAW=1 \
  NIRICAPTURE_TEST_EXPORT=1 niricapture --capture-region
NIRICAPTURE_TEST_SELECT=20,380,800,500 NIRICAPTURE_TEST_OCR=1 \
  niricapture --capture-region
```

The geometry hook started with a 1100×700 region, dragged its bottom-right
handle out and produced a 1350×820 selection, proving that a selection can be
expanded again after shrinking. The draw hook produced a rectangle, arrow,
numbered marker and pixelated redaction in the saved 1100×700 PNG. The OCR
hook opened the real selectable result panel.

## Standalone editor (0.5.0)

`tests/editor-window-smoke.cpp` launches `EditorWindow` offscreen, creates a
400×300 canvas, and verifies the window renders. Live checks:

```bash
niricapture --editor          # New/Open start page
niricapture --file image.png  # direct image editing
niricapture --capture-window --window-id 44
```

The launcher entry `NiriCapture Editor` runs `niricapture --editor`; it is
installed as `$XDG_DATA_HOME/applications/niricapture-editor.desktop` and was
validated with `desktop-file-validate`.

Window capture uses `Ctrl+F1` in the example/user Niri bindings. The window
image opens in the same modular editor, where canvas zoom/fit, padding/size,
transparent/white/dark/gradient backgrounds, save/copy/pin and OCR are
available.

## Configuration checks

`tests/config-smoke.cpp` parses a config with custom tools/commands/shortcuts,
palette, canvas, theme, OCR and screenshot paths, and verifies each value is
applied. `--print-config` creates the default file and prints its contents plus
path. Live checks used `NIRICAPTURE_CONFIG=/tmp/nc-config.json` with a reduced
toolbar (select/text/marker), green default style, two palette colors, padding
24 and an Aurora canvas; the standalone editor and the in-place overlay both
honored it.


## Config v2 checks

`tests/config-smoke.cpp` now validates:

- JSON parse and defaults inheritance;
- v1 → v2 migration;
- newer schema rejection and `writable() == false`;
- unknown keys at root and inside `canvas` / `editor` / `palette` / `ocr` /
  `paths` preserved through `toJson()`;
- atomic `QSaveFile` save and reload from a nested directory.


## 1.0 settings checks

- `niricapture --settings` stays alive in a normal Wayland session; settings
  fields load from config v2 and save atomically.
- Eyedropper is covered by `tests/tool-smoke.cpp` colour-sampling assertions.


## 1.0 resize/font checks

- `Shift` preserves a rectangle's aspect ratio while dragging a resize handle.
- `Ctrl` keeps the annotation centre fixed.
- `annotationTextFont` accepts a custom family and `editor.font` is parsed and
  serialized by config v2.
- Toolbar hover labels are painted above the button (or below when the toolbar
  is too close to the top edge).


## Material 3 settings window (2026-11-10)

- `QQuickWidget` + Qt Quick Controls Material style renders under
  `QT_QPA_PLATFORM=offscreen`, so the existing `SettingsWindow` smoke still
  passes without a compositor.
- Live X11/Xvfb render checks at 920×700 and 946×720 covered all six pages,
  hover states, the dirty-state footer and the read-only banner.
- Save roundtrip: toggling a tool off and saving wrote a config v2 file whose
  `tools` array no longer contained `select`; the snackbar showed the path.
- Read-only config (`version: 99`) disabled Save, dimmed the pages and showed
  the error-container banner.
- Closing with unsaved changes opened the in-app discard dialog; “继续编辑”
  stays, “放弃并关闭” exits.
- The in-app MD3 colour picker changed the accent and the saved config
  (`#2f6fe6`); the directory picker opens the widget `QFileDialog`.
- Known: `QtQuick.Dialogs` cannot open its non-native dialogs inside a
  `QQuickWidget` (“parent window … is not a QQuickWindow”), which is why the
  colour picker is an in-app popup and the folder picker is a widget dialog.

## Material 3 Widgets chrome (2026-11-11)

- The editor home/top bar, font picker, file dialogs, OCR dialogs, context
  menus and message boxes use the shared `md3WidgetStyleSheet()` tokens and the
  dark application palette.
- Live X11/Xvfb checks: editor home and canvas render with the MD3 top bar and
  tonal/filled home actions; the font picker shows the rounded container,
  outlined combo with a visible arrow and filled accept button; the non-native
  QFileDialog renders dark with readable labels and a filled default button.
- File dialogs are created with `DontUseNativeDialog` so the stylesheet applies;
  Qt native/portal dialogs would ignore it.
- The floating annotation toolbar paints with MD3 surfaces, outline variants,
  primary-container active pills, MD3 tooltips and the themed colour/width
  popup; the in-place overlay and the standalone editor share this component.

## Snapping checks

- Edge snapping produces guide lines and aligns to the image/annotation bounds.
- 45° snapping constrains a line endpoint around its anchor.
- `Alt` toggles snapping; a line drawn with snapping off retains its raw angle.
- Wheel steps through the configured line-width presets.


## Font dialog checks

- The standalone font action opens the embedded picker, not a portal dialog.
- The in-place screenshot overlay opens the same picker and remains responsive.
- Picker labels and buttons are Chinese; `Ctrl+Shift+F` opens it directly.
