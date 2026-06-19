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

