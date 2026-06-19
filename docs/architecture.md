# Architecture

LabelMinus Qt Port is organized around a small set of boundaries so the project can grow without turning the main window into the whole application.

## Layers

### `src/core`

Core code should not depend on widgets. It contains:

- `Project`, `ImageEntry`, `Label`: project data model.
- `LabelPlusDocument`: LabelPlus text parsing and serialization.
- `AppPreferences`: typed access to `preference.json`.
- `UndoStack`: command-based undo infrastructure.

Keep LabelPlus file-format details here, not in UI code.

### `src/ui`

Qt Widgets classes live here:

- `MainWindow`: top-level layout and UI wiring.
- `ImageCanvas`: image preview, marker drawing, click-to-label and view interaction.
- `LabelTableModel`: table model for current image labels.
- `GroupFilterComboBox`: multi-select group filtering widget.

UI classes may coordinate core objects, but should avoid embedding file-format parsing or platform-service code.

### `src/services`

External integrations belong here:

- Archive reading.
- OCR subprocesses.
- Future platform-specific desktop integration.

## Preferences

Runtime UI tuning lives in `preference.json` and is read through `AppPreferences`.

Current preferences:

- `labelMarker.diameter`: marker diameter in screen pixels; floating-point values are accepted.
- `labelMarker.fontPointSize`: marker number size as a Qt font point size; floating-point values are accepted.
- `labelTable.maxTextRows`: maximum visible wrapped text lines for each label table row.
- `input.moveLabelModifier`: modifier key or key combination used to drag label markers.
- `groupStyles`: per-group marker and text color styles assigned by group index.

Do not read `preference.json` directly from UI classes except through `AppPreferences`.
Invalid or unreadable preference values should fall back to defaults and be reported through non-blocking UI, such as
the status bar.

## Group Styles

Group style assignment is index-based:

- Group 1 uses `groupStyles[0]`.
- Group 2 uses `groupStyles[1]`.
- Group 3 uses `groupStyles[2]`.

Each style can define:

- `groupColor`: color for marker fill and group text UI.
- `markerDiameter`: marker size in screen pixels.
- `fontPointSize`: marker number size as a Qt font point size.
- `markerStyle`: `circle` or `square`.

If there is no configured style:

- Image markers use a black circular marker with the default marker size.
- Text UI keeps the default text color.

Color consumers:

- `ImageCanvas`: marker fill color.
- `MainWindow`: insert-group combo box text color.
- `LabelTableModel`: group column text color.
- `GroupFilterComboBox`: group filter menu item text color.

## i18n

Use Qt's standard workflow:

- Mark user-visible strings with `tr()`.
- Keep Simplified Chinese and English TS files in `translations/`.
- Use `release_translations` to generate QM files.
- `main.cpp` installs `QTranslator` before creating UI widgets.

Run `scripts/check_translations.sh` after changing UI text.

## Undo

Use `UndoStack` for every reversible project edit. Current covered commands include adding labels, moving labels,
editing label text and changing label groups. Future operations such as deleting labels, bulk edits or OCR writes
should be added as commands instead of separate ad hoc state.
