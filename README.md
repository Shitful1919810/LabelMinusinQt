# LabelMinus Qt Port

This branch is a C++/Qt 6 port of the original WPF application.

The current application can open an existing classic LabelPlus `.txt` project, load images from the text file's folder, add labels by clicking the image preview, edit label text and groups, filter labels by group, and save the project back to LabelPlus text.

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

Open a LabelPlus text project directly from the command line:

```bash
./build/linux/debug/src/labelminus /path/to/project.txt
```

## Developer Checks

```bash
scripts/check_translations.sh
cmake --build --preset linux-debug --target release_translations
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Format C++ sources with:

```bash
find src tests -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 | xargs -0 clang-format -i
```

## Preferences

Runtime UI tuning lives in `preference.json`.

Current options:

```json
{
  "labelMarker": {
    "diameter": 4.0,
    "fontPointSize": 2.5
  },
  "labelTable": {
    "maxTextRows": 3
  },
  "groupColors": [
    "#ff3835",
    "#5ba8ec",
    "#a3d100"
  ]
}
```

`labelMarker.diameter` is a screen pixel size. `labelMarker.fontPointSize` is a Qt font point size. Both accept
floating-point values, and markers keep the same on-screen size when the image preview is zoomed.
`labelTable.maxTextRows` caps automatic label table row heights after text wrapping.

Group colors are assigned by group index. If a group has no configured color, image markers use black and text UI keeps the default text color.

## Current Scope

- `src/core`: platform-independent model, LabelPlus parser/serializer, preferences and undo infrastructure.
- `src/services`: archive and OCR integration points.
- `src/ui`: Qt Widgets user interface.
- `translations`: Qt Linguist `.ts` files for Simplified Chinese and English.
- `tests`: Qt Test based unit tests.

Implemented first-stage UI:

- Open/save existing LabelPlus text projects.
- Image preview with zoom, page selection and click-to-add labels.
- Insert-group selection for new labels.
- Colored label markers based on group.
- Label table with group filtering.
- Current-label text and group editing.
- Unsaved-change prompt on close/open.
- Simple command-based undo stack, currently covering added labels.
