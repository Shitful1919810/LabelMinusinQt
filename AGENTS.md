# Agent Guidance

This branch is a C++/Qt port of LabelMinus. Treat the old WPF implementation as removed on purpose.

## Encoding

The project may contain Chinese UI strings, documentation and file names. Keep text files in UTF-8 and avoid broad search-and-replace operations that might damage localized text.

## Build

Use the CMake presets when possible:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

If the local compiler is routed through ccache and CMake fails while detecting `Threads`, retry with:

```bash
cmake -E env CCACHE_DISABLE=1 cmake --preset linux-debug
cmake -E env CCACHE_DISABLE=1 cmake --build --preset linux-debug
cmake -E env CCACHE_DISABLE=1 ctest --preset linux-debug
```

## Porting Direction

- Prefer Qt Widgets for the main desktop workflow.
- Keep platform-independent logic under `src/core`.
- Put project workflow, session state, OS, archive, OCR and process integration under `src/services`.
- Keep Qt UI classes under `src/ui`.
- Avoid reintroducing WPF, .NET or Windows-only dependencies on this branch.
- Avoid Qt GPL-only modules unless the user explicitly approves the license impact.

## Required Conventions

- Follow `CONTRIBUTING.md` and `docs/architecture.md`.
- New user-visible UI text must use `tr()`.
- When adding or changing `tr()` strings, update both `translations/labelminus_zh_CN.ts` and `translations/labelminus_en_US.ts`.
- Run `scripts/check_translations.sh` after UI text changes.
- Configurable UI behavior should go through `AppPreferences` and `preference.json`.
- Reversible project edits must use the Qt-backed `UndoStack`; add undo and redo behavior in the same change that introduces the edit.
- Label edits should go through `LabelEditController` rather than adding new label mutation paths in `MainWindow`.
- Keep `MainWindow` focused on UI orchestration; put project workflow, session state and mutation logic in services.
- Current-page label changes should update table/marker state in place and must not reload the image unless the current image actually changes.
- New Qt modules and third-party dependencies must be checked for license compatibility and documented.
- User-configurable shortcuts should go through `AppPreferences`, `preference.json` and the preference dialog.
- The Windows `LabelMinusStatic` target is experimental and local-only; do not make it an official release artifact without Qt license review.
- Do not commit bundled Qt SDK files, Qt source code or Qt runtime binaries into the repository.
