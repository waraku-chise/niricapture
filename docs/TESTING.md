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

The interaction suite also OCRs a real `CaptureEditor` region, waits for the
worker, verifies the selectable result panel contains the recognized text, and
checks that `Esc` closes the panel before the editor.

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
