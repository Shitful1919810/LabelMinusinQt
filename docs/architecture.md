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
- `groupColors`

Do not read `preference.json` directly from UI classes except through `AppPreferences`.
Invalid or unreadable preference values should fall back to defaults and be reported through non-blocking UI, such as
the status bar.

## Group Colors

Group color assignment is index-based:

- Group 1 uses `groupColors[0]`.
- Group 2 uses `groupColors[1]`.
- Group 3 uses `groupColors[2]`.

If there is no configured color:

- Image markers use black.
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

Use `UndoStack` for reversible operations. The current first command is "add label"; future operations such as deleting, moving, editing text and changing group should be added as commands instead of separate ad hoc state.
