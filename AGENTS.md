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
- Put OS, archive, OCR and process integration under `src/services`.
- Keep Qt UI classes under `src/ui`.
- Avoid reintroducing WPF, .NET or Windows-only dependencies on this branch.

## Required Conventions

- Follow `CONTRIBUTING.md` and `docs/architecture.md`.
- New user-visible UI text must use `tr()`.
- When adding or changing `tr()` strings, update both `translations/labelminus_zh_CN.ts` and `translations/labelminus_en_US.ts`.
- Run `scripts/check_translations.sh` after UI text changes.
- Configurable UI behavior should go through `AppPreferences` and `preference.json`.
- Reversible editing operations should use `UndoStack`.
