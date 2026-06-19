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
- `src/services`: external integrations such as OCR, archive and process handling.
- `translations`: Qt `.ts` files.
- `preference.json`: runtime UI tuning defaults.

See `docs/architecture.md` for more detail.

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
- Group colors are assigned by group index. If a group has no configured color, text UI uses the default color and image markers use black.

## Undo

- New reversible project edits must use `UndoStack`.
- When adding a feature that changes labels, groups, pages or project data, add the undo command in the same change.
- Avoid adding one-off undo state in UI code.

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
