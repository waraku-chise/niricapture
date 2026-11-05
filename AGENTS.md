# NiriCapture agent guide

NiriCapture is a standalone Qt 6 screenshot tool for **Niri + DMS** on Arch
Linux. It is not a DMS/Quickshell plugin and does not target Noctalia or
Hyprland.

## Principles

- Niri only: documented JSON IPC and compositor actions, never a guessed API.
- Reuse DMS's freeze path (`dms screenshot output`) for region/fullscreen.
- Window capture must be Niri's independent window render. Never fall back to
  cropping the desktop.
- The editor is a separate process and must survive a DMS shell restart.
- Non-destructive annotations; redaction/blur replace exported source pixels.
- Fail closed: no overlay before all required frozen frames exist.
- No changes to Niri/DMS core or user configuration.

## Layout

| Path | Purpose |
|---|---|
| `src/main.cpp` | CLI, mode dispatch, instance lock, layer-shell setup |
| `src/desktop-capture.*` | Niri outputs, DMS frames, multi-output composition |
| `src/region-selector.*` | one layer surface per output, shared selection |
| `src/capture.*` | rendering, clipboard, OCR, source effects |
| `src/editor.*` | annotation editor, toolbar, export |
| `src/surface-capture.cpp` | Niri native window action and async PNG wait |
| `src/pin.*` | always-visible pinned images |
| `tests/` | offscreen Qt smoke, composition/OCR, Python contracts |
| `packaging/` | installer, uninstaller, PKGBUILD, Niri keybinds |
| `docs/` | source research and test record |

## Commands

```bash
make build
make smoke
make check                 # contracts + smoke + lint
python3 -m unittest discover -s tests -p 'test_*.py' -v
./packaging/install-local.sh --check
```

Run tests after each meaningful change. Do not rerun an unchanged successful
check.

## Hard constraints

- Niri logical output geometry is already transformed.
- `screenshot-window --id` writes its PNG asynchronously after the CLI exits.
  Success requires a readable, non-empty image, not exit code 0.
- Niri always overwrites the clipboard with the raw window image. Document it;
  never claim restoration.
- Never crop monitor pixels as a fallback for failed window capture.
- Never pass external values through `sh -c`.
- DMS CLI calls use argument arrays; no shell interpolation.
- Install/uninstall must not edit Niri or DMS configuration/state.
- Preserve `LICENSE`, `NOTICE`, `LICENSES/MIT-nirisnap.txt` and
  `assets/OFL.txt`.

## Release

Version lives in `CMakeLists.txt`. Before tagging `vX.Y.Z`, run `make check`,
`install-local.sh --check`, stage a CMake installation, and update
`docs/TESTING.md`.
