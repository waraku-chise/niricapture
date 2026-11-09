# Configuration

NiriCapture reads a JSON file at:

```text
$XDG_CONFIG_HOME/niricapture/config.json
# normally ~/.config/niricapture/config.json
```

If it does not exist, it is created with documented defaults. The settings
window (`niricapture --settings`) edits this file and saves it atomically. The path can be
overridden with:

```bash
NIRICAPTURE_CONFIG=/path/to/config.json niricapture --editor
niricapture --config /path/to/config.json --editor
niricapture --print-config
```

A malformed file is reported on stderr and the built-in defaults are used; the
file is not overwritten. Unknown keys are preserved when the file is saved by
the settings UI or another writer.

## Schema and migration

The current schema is `version: 2`. Missing versions are treated as v1 and
migrated in memory; the next save writes v2. A file declaring a newer schema is
rejected with an explicit error, and defaults are used for that process.

Saving uses `QSaveFile` (write to a temporary file, then atomic rename), so a
crash or full disk does not leave a truncated config. Errors include the path
and the Qt error string.

## Full example

```json
{
  "version": 2,
  "tools": [
    "select", "pen", "arrow", "line", "rectangle", "ellipse",
    "text", "highlighter", "mosaic", "blur", "marker", "eyedropper"
  ],
  "commands": [
    "palette", "font", "undo", "redo", "ocr", "copy", "save", "close"
  ],
  "shortcuts": {
    "select": "V",
    "pen": "F",
    "arrow": "A",
    "line": "L",
    "rectangle": "R",
    "ellipse": "E",
    "text": "T",
    "highlighter": "H",
    "mosaic": "D",
    "blur": "B",
    "marker": "C",
    "eyedropper": "I"
  },
  "palette": {
    "colors": ["#ff375f", "#ff9f0a", "#ffd60a", "#30d158",
               "#0a84ff", "#bf5af2", "#ffffff", "#000000"],
    "widths": [2, 4, 8, 12]
  },
  "canvas": {
    "width": 1600,
    "height": 900,
    "padding": 0,
    "background": "transparent",
    "background_color": "#151922",
    "fit_on_open": true
  },
  "editor": {
    "accent": "#2f6fed",
    "background": "#0f1116",
    "panel": "#151922",
    "checker_light": "#242a35",
    "checker_dark": "#1b1f27",
    "default_color": "#ff375f",
    "default_width": 4,
    "font": "Neucha"
  },
  "ocr": {"languages": ""},
  "paths": {"screenshots": ""}
}
```

## tools

Ordered list of tool ids. Only listed tools appear in the toolbar and receive
their configured shortcut. Available ids:

```text
select, pen, arrow, line, rectangle, ellipse,
text, highlighter, mosaic, blur, marker, eyedropper
```

If the list is empty or omitted, all built-in tools are shown in registration
order. Omit a tool to hide it without deleting code.

## commands

Ordered toolbar command ids:

```text
palette, undo, redo, ocr, copy, save, close
```

The top bar of the standalone editor keeps Home/Open/Save/Copy/Pin/Canvas/Close
regardless of this list; `commands` controls the annotation toolbar.

## shortcuts

Tool id to a Qt key sequence. Examples: `"V"`, `"Ctrl+Shift+P"`. The shortcut
only changes the tool switch; global actions (`Ctrl+C`, `Ctrl+S`, `Ctrl+Z`,
`Esc`) remain fixed.

## palette

`colors` is a list of `#rrggbb` values (the default palette has 36 colours in
a 9×4 grid); `widths` is a list of positive integer line widths. They drive the colour/width popup and the colour/size actions for
the selected annotation. `editor.default_color` and `editor.default_width` set
the initial style.

## canvas

Applies to new canvases and images opened in the standalone editor:

- `width`, `height`: default New canvas size, 1–20000.
- `padding`: transparent/background margin around the image, 0–512.
- `background`: `transparent`, `white`, `dark`, `aurora`, `sunset`, `lagoon`,
  or `violet`.
- `background_color`: used when `background` is `transparent`, `white`, or
  `dark`; a named `white`/`dark` gets a sensible default if this key is
  omitted.
- `fit_on_open`: reserved for fit-on-open behavior.

The Canvas dialog in the editor changes the current document interactively;
the JSON values are the starting defaults for new documents.

## editor

Theme colors for the standalone editor. `accent` is used for selected tool
states, `checker_light`/`checker_dark` for the transparency checkerboard, and
`default_color`/`default_width` for the initial annotation style, and `font`
for the initial text font (the toolbar "文字字体" action can override it).

## ocr

`languages` is passed to Tesseract, e.g. `"eng"` or `"eng+chi_sim"`. If empty,
the Tesseract language autodetection is used. `NIRICAPTURE_OCR_LANGS` overrides
the file.

## paths

`screenshots` sets the default save directory. `NIRICAPTURE_SCREENSHOT_DIR`
overrides the file.

## Keybindings

Niri keybindings are **not** in this file. They live in the Niri config (for
example `custom-screenshot.kdl`), because the compositor owns global shortcuts:

```kdl
Ctrl+F1 repeat=false hotkey-overlay-title="Screenshot: 窗口 (NiriCapture)" {
    spawn "env" "NIRICAPTURE_OCR_LANGS=eng+chi_sim" \
        "/home/chise/.local/bin/niricapture" "--capture-window";
}
```
