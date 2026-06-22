# Architecture

LabelMinus Qt Port is organized around a small set of boundaries so the project can grow without turning the main window into the whole application.

## Layers

### `src/core`

Core code should not depend on widgets. It contains:

- `Project`, `ImageEntry`, `Label`: project data model.
- `LabelPlusDocument`: LabelPlus text parsing and serialization.
- `AppPreferences`: typed access to `preference.json`.
- `ApplicationTheme`: built-in application stylesheet theme registry.
- `UndoStack`: thin wrapper around Qt `QUndoCommand` / `QUndoStack` for undo and redo infrastructure.

Keep LabelPlus file-format details here, not in UI code.

### `src/ui`

Qt Widgets classes live here:

- `MainWindow`: top-level layout and UI wiring.
- `ImageCanvas`: image preview, marker drawing, click-to-label and view interaction.
- `MainWindowShortcutController`: main-window keyboard shortcut routing. It maps configured shortcuts to callbacks but
  does not mutate project data or widgets directly.
- `CanvasLabelTextEditController`: lifecycle controller for marker-adjacent label text editing on the image canvas.
- `CanvasLabelTextEditor`: temporary marker-adjacent text editor widget for canvas-side label text edits.
- `ProjectMergeDialog`: conflict-resolution dialog for offline page-based LabelPlus project merges.
- `LabelTableModel`: table model for current image labels.
- `GroupFilterComboBox`: multi-select group filtering widget.
- `ThemeManager`: application-level Qt stylesheet theme loading.

UI classes may coordinate core objects, but should avoid embedding file-format parsing, project workflow, label mutation
rules or platform-service code. `MainWindow` should stay close to UI orchestration: creating controls, connecting
signals, calling services and reflecting service results in widgets.

Reusable or stateful widget fragments, such as floating editors and custom controls, should live in their own UI classes
instead of being built inline in `MainWindow`.

Qt ownership is allowed and expected, but cached pointers need clear lifetime boundaries. Composite widgets that connect
signals from child/internal widgets should disconnect those connections before destruction starts tearing down owned
objects. If a class caches raw pointers owned by a Qt container or parent object, such as `QGraphicsScene` items, clear
or null the cache before the owner clears/destructs. For cached `QObject`/`QWidget` references used from callbacks,
queued events or delayed deletion paths, prefer `QPointer` so the pointer becomes null when the object is destroyed.

Global shortcut matching should stay in `MainWindowShortcutController`. `MainWindow` may provide callbacks that preserve
editing state before/after navigation, but it should not grow another parallel shortcut parser.

`LabelTableModel` is a view model: it may validate edits and emit edit requests, but it must not mutate `Label` objects
directly. Route label and group mutations through `LabelEditController` so undo, dirty state and UI refresh stay
consistent.

### `src/services`

Application services belong here. They can use QtCore services such as file IO, timers and `QSettings`, but should avoid
Qt Widgets. Current services include:

- `ProjectController`: owns the open `Project`, project dirty state, file load/save and auto-backup writes.
- `ProjectMergeService`: loads multiple LabelPlus text projects and prepares page-based merge plans.
- `LabelEditController`: applies label/group edits and registers undo commands without depending on widgets.
- `LabelNavigator`: finds previous/next visible labels across pages using project data and the active group filter.
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

## Dependency And License Boundaries

The current application is intended to stay on Qt modules that are available under LGPL-compatible open-source use. The
runtime target currently links:

- `Qt6::Core`
- `Qt6::Gui`
- `Qt6::Widgets`

When Qt Svg is available, the build also links `Qt6::Svg` so the bundled Breeze stylesheet SVG icons work more
completely. The application should still build without Qt Svg.

Before adding any Qt module, check the official Qt licensing documentation. Do not add GPL-only Qt modules unless the
project explicitly accepts the resulting GPL-oriented distribution requirements. Examples of modules that require extra
care include Qt Graphs, Qt GRPC, Qt HTTP Server, Qt MQTT, Qt Virtual Keyboard and Qt Wayland Compositor.

Release packaging should prefer dynamic linking to Qt. The optional Windows `LabelMinusStatic` target is for local
experiments with a static Qt build only; it is not the default release path. Do not publish static Qt binaries without a
Qt license review. Source repositories should not vendor Qt SDK files, Qt source code or Qt runtime binaries. Binary
releases need third-party notices covering Qt and any other bundled dependencies.

The repository bundles BreezeStyleSheets resources under `resources/themes/breeze`. They are MIT-licensed, and the SVG
icon assets carry the Apache License 2.0 notice included with the upstream project. Keep `THIRD_PARTY_NOTICES.md` and
the local license files in sync when updating those resources.

## Preferences

Runtime UI tuning lives in `preference.json` and is read through `AppPreferences`.

Current preferences:

- `appearance.style`: optional Qt widget style name. Empty means the platform/system default is used. Available values are discovered with `QStyleFactory::keys()` at runtime.
- `appearance.theme`: optional built-in Breeze stylesheet theme. Empty means no application stylesheet. Current built-in values are `breezeDark` and `breezeLight`. This is layered on top of `appearance.style`, so Qt styles and Breeze QSS themes coexist.
- `labelMarker.diameter`: marker diameter in screen pixels; floating-point values are accepted.
- `labelMarker.fontPointSize`: marker number size as a Qt font point size; floating-point values are accepted.
- `labelTable.maxTextRows`: maximum visible wrapped text lines for each label table row.
- `labelTable.fontFamily`: optional label table font family; an empty value keeps the Qt/system default.
- `labelTable.fontPointSize`: optional label table font point size; `0` keeps the Qt/system default.
- `labelTextEditor.fontFamily`: optional bottom text editor font family; an empty value keeps the Qt/system default.
- `labelTextEditor.fontPointSize`: optional bottom text editor font point size; `0` keeps the Qt/system default.
- `markerTextBubble.fontFamily`: optional marker text bubble font family; an empty value keeps the Qt/system default.
- `markerTextBubble.fontPointSize`: optional marker text bubble font point size; `0` keeps the Qt/system default.
- `markerTextBubble.opacity`: marker text bubble opacity from `0.0` to `1.0`.
- `input.moveLabelModifier`: modifier key or key combination used to drag label markers.
- `input.previousLabelModifier`: modifier key or key combination used with `input.nextLabelShortcut` to select the previous visible label.
- `input.nextLabelShortcut`: main-window shortcut used to select the next visible label.
- `input.alternatePreviousLabelShortcut`: additional main-window shortcut used to select the previous visible label.
- `input.alternateNextLabelShortcut`: additional main-window shortcut used to select the next visible label.
- `input.previousPageShortcut`: main-window shortcut used to move to the previous image page.
- `input.nextPageShortcut`: main-window shortcut used to move to the next image page.
- `input.editLabelTextShortcut`: shortcut used in the label table to edit the current label text.
- `input.commitLabelTextShortcut`: shortcut used in the label text editor delegate to commit and close inline editing.
- `input.undoShortcut`: undo shortcut in Qt portable key sequence text format.
- `input.redoShortcut`: redo shortcut in Qt portable key sequence text format.
- `backupPath`: auto-backup directory; relative paths are resolved from the open project file directory.
- `backupIntervalSeconds`: auto-backup check interval.
- `groupStyles`: per-group marker and text color styles assigned by group index.

Do not read `preference.json` directly from UI classes except through `AppPreferences`.
Invalid or unreadable preference values should fall back to defaults and be reported through non-blocking UI, such as
the status bar.

The preference dialog should update the same JSON shape that `AppPreferences` reads and should reuse `AppPreferences`
defaults instead of duplicating fallback values. User-facing preference text must still go through `tr()` and both
translation files.

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

Use the Qt-backed `UndoStack` wrapper for every reversible project edit. It stores commands as `QUndoCommand` instances
inside a `QUndoStack`, so new edit commands must provide both undo and redo behavior. Current covered commands include
adding labels, moving labels, editing label text, changing label groups, adding/removing groups, deleting labels,
reordering labels and bulk group changes. Future operations such as OCR writes should be added as commands instead of
separate ad hoc state.

Undo commands should be registered close to the code that performs the edit. Label edits should go through
`LabelEditController`, which routes normal edits and undo replay through shared apply functions. Future non-label
project edits should follow the same pattern instead of adding one-off undo state in UI code.

## Session State

Local, machine-specific state is stored with `QSettings`, not in `preference.json`.

Current session state includes:

- Main window geometry and splitter positions.
- Recent project file paths.
- Last viewed page for each project file.
- Image zoom percentage and normalized view center.
- Last selected label index.

`SessionStateStore` clamps and validates restored values through `MainWindow`, so external edits to a project file, such
as deleting pages, should not crash the next launch.
