# Contributing

This branch is a C++/Qt 6 port of LabelMinus. Keep changes aligned with the current architecture and cross-platform goal.

## Code Style

- Use C++20 and Qt 6 Widgets.
- Class names use `PascalCase`.
- Functions use `camelCase`.
- Member variables use the `m_` prefix.
- Prefer small classes and explicit ownership through Qt parent/child relationships.
- Run `clang-format` with the repository `.clang-format` before committing substantial C++ changes.
- Do not introduce WPF, .NET or Windows-only dependencies on this branch.

## Architecture Boundaries

- `src/core`: platform-independent models, parsing, preferences and undo infrastructure.
- `src/ui`: Qt Widgets UI classes.
- `src/services`: application services such as project workflow, session persistence, backup, OCR, archive and process handling.
- `translations`: Qt `.ts` files.
- `preference.json`: runtime UI tuning defaults.

See `docs/architecture.md` for more detail.

## Qt Licensing Constraints

- Keep the application on LGPL-available Qt modules unless the project intentionally changes to a GPL-compatible release strategy.
- Current required Qt dependencies should stay limited to `Qt6::Core`, `Qt6::Gui` and `Qt6::Widgets` unless a new module is reviewed. `Qt6::Svg` is an optional enhancement for bundled stylesheet icons.
- Do not introduce Qt GPL-only modules without documenting the license impact and getting an explicit project decision.
- Prefer dynamic linking for Qt in release packaging.
- The Windows `LabelMinusStatic` target is experimental and local-only. Do not publish static Qt binaries without a Qt license review.
- Do not commit Qt source code, Qt SDK files or bundled Qt binaries into this repository.
- Binary releases must include Qt license notices, Qt module/version information and third-party dependency notices.
- When adding a third-party dependency, document its license and keep it compatible with the intended project license.
- Bundled BreezeStyleSheets resources must keep their local license files and `THIRD_PARTY_NOTICES.md` entry in sync.

## UI And Workflow Separation

- Keep `MainWindow` as an orchestration layer for menus, widgets, signal/slot wiring and UI feedback.
- Do not add new file-format parsing, project workflow, label mutation or session persistence logic directly to `MainWindow`.
- Put non-widget workflow logic in `src/services`; UI classes can call services and then refresh controls.
- Table models should emit edit requests and leave project mutation to controllers.
- When a current-page label edit only changes marker/table/editor state, refresh labels in place instead of reloading the image.
- Reserve full image reloads for real page/image changes, project open/creation, and explicit session restore paths.

## UI Text And i18n

- User-visible UI strings must use `tr()`.
- When adding or changing a `tr()` string, update both:
  - `translations/labelminus_zh_CN.ts`
  - `translations/labelminus_en_US.ts`
- Then run:

```bash
scripts/check_translations.sh
cmake --build --preset linux-debug --target release_translations
```

## Preferences

- Configurable UI behavior should go through `AppPreferences`.
- Defaults should live in `preference.json`.
- Preference dialog controls should preserve the same JSON shape that `AppPreferences` reads.
- Preference dialog fallback values should come from `AppPreferences` defaults instead of another hand-written copy.
- Group colors are assigned by group index. If a group has no configured color, text UI uses the default color and image markers use black.

## Undo

- New reversible project edits must use the Qt-backed `UndoStack` wrapper, which is implemented with `QUndoCommand` and `QUndoStack`.
- When adding a feature that changes labels, groups, pages or project data, add undo and redo behavior in the same change.
- Avoid adding one-off undo state in UI code.
- Label and group edits should go through `LabelEditController` so normal edits and undo replay use the same apply helpers.
- Prefer small undo callbacks that call shared apply helpers, so table edits, canvas edits and future batch tools keep the same behavior.
- User-configurable shortcuts, including undo/redo shortcuts, should go through `AppPreferences`, `preference.json` and the preference dialog.

## Session And Local State

- Store machine-local layout and per-project resume state with `QSettings` through `SessionStateStore`.
- Do not write local window geometry, splitter positions or last-viewed page back to `preference.json`.

## Verification

For normal development:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
scripts/check_translations.sh
```

If ccache interferes with CMake compiler detection:

```bash
cmake -E env CCACHE_DISABLE=1 cmake --preset linux-debug
cmake -E env CCACHE_DISABLE=1 cmake --build --preset linux-debug
cmake -E env CCACHE_DISABLE=1 ctest --preset linux-debug
```
