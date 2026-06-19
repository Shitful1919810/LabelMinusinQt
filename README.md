# LabelMinus Qt Port

This branch is a C++/Qt port of the original WPF application.

## Build

Install Qt 6, CMake, Ninja and a C++20 compiler, then run the preset for your host platform.

Linux:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Windows:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

macOS:

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug
```

If CMake fails while detecting `Threads` because the compiler is routed through ccache, retry the same commands through `cmake -E env CCACHE_DISABLE=1`.

The application target is `LabelMinus`. Linux builds produce a `labelminus` binary, Windows builds produce a GUI executable, and macOS builds produce an app bundle.

## Preferences

Runtime UI tuning lives in `preference.json`. The current options configure image label markers:

```json
{
  "labelMarker": {
    "diameter": 36,
    "fontPointSize": 10
  }
}
```

## Current Scope

The current code is a clean Qt Widgets skeleton. It opens a single image in a `QGraphicsView`-based canvas and provides the project structure for the larger port:

- `src/core`: platform-independent domain model.
- `src/services`: archive and OCR integration points.
- `src/ui`: Qt Widgets user interface.
- `resources`: assets and OCR scripts to be added later.
- `tests`: Qt Test based unit tests.
