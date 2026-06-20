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

UI classes may coordinate core objects, but should avoid embedding file-format parsing, project workflow, label mutation
rules or platform-service code. `MainWindow` should stay close to UI orchestration: creating controls, connecting
signals, calling services and reflecting service results in widgets.

### `src/services`

Application services belong here. They can use QtCore services such as file IO, timers and `QSettings`, but should avoid
Qt Widgets. Current services include:

- `ProjectController`: owns the open `Project`, project dirty state, file load/save and auto-backup writes.
- `LabelEditController`: applies label edits and registers undo commands without depending on widgets.
- `SessionStateStore`: persists local window layout and per-project session state through `QSettings`.
- Archive reading.
- OCR subprocesses.
- Future platform-specific desktop integration.

Keep workflow/state persistence code here when it would otherwise make `MainWindow` responsible for non-UI details.

## UI Refresh Rules

Avoid using full image reloads as a generic refresh tool. `ImageCanvas::setImage()` rebuilds the scene for a page and can
change the user's current view position. Use it only when the current image actually changes, such as opening a project,
switching pages or restoring a saved session.

For edits on the current page, keep the image scene stable and refresh only the affected view state:

- Use label/model refresh helpers for marker, table and editor updates.
- Undo replay on the current page should preserve zoom and view center.
- Group, preference and label edits should not call full image refresh just to repaint markers.

## Preferences

Runtime UI tuning lives in `preference.json` and is read through `AppPreferences`.

Current preferences:

- `labelMarker.diameter`: marker diameter in screen pixels; floating-point values are accepted.
- `labelMarker.fontPointSize`: marker number size as a Qt font point size; floating-point values are accepted.
- `labelTable.maxTextRows`: maximum visible wrapped text lines for each label table row.
- `labelTable.fontFamily`: optional label table font family; an empty value keeps the Qt/system default.
- `labelTable.fontPointSize`: optional label table font point size; `0` keeps the Qt/system default.
- `labelTextEditor.fontFamily`: optional bottom text editor font family; an empty value keeps the Qt/system default.
- `labelTextEditor.fontPointSize`: optional bottom text editor font point size; `0` keeps the Qt/system default.
- `input.moveLabelModifier`: modifier key or key combination used to drag label markers.
- `backupPath`: auto-backup directory; relative paths are resolved from the open project file directory.
- `backupIntervalSeconds`: auto-backup check interval.
- `groupStyles`: per-group marker and text color styles assigned by group index.

Do not read `preference.json` directly from UI classes except through `AppPreferences`.
Invalid or unreadable preference values should fall back to defaults and be reported through non-blocking UI, such as
the status bar.

The preference dialog should update the same JSON shape that `AppPreferences` reads. User-facing preference text must
still go through `tr()` and both translation files.

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
editing label text, changing label groups, deleting labels, reordering labels and bulk group changes. Future operations
such as OCR writes should be added as commands instead of separate ad hoc state.

Undo commands should be registered close to the code that performs the edit. Label edits should go through
`LabelEditController`, which routes normal edits and undo replay through shared apply functions. Future non-label
project edits should follow the same pattern instead of adding one-off undo state in UI code.

## Session State

Local, machine-specific state is stored with `QSettings`, not in `preference.json`.

Current session state includes:

- Main window geometry and splitter positions.
- Last viewed page for each project file.
- Image zoom percentage and normalized view center.
- Last selected label index.

`SessionStateStore` clamps and validates restored values through `MainWindow`, so external edits to a project file, such
as deleting pages, should not crash the next launch.
