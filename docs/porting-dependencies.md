# Porting Dependencies

Required:

- Qt 6.5 or newer: `Core`, `Gui`, `Widgets`.
- CMake 3.24 or newer.
- Ninja or another CMake generator.
- A C++20 compiler.

Recommended platform toolchains:

- Linux: GCC or Clang, Ninja, Qt 6 development packages.
- Windows: Visual Studio 2022 Build Tools or full Visual Studio, Ninja, Qt 6 for MSVC.
- macOS: Xcode Command Line Tools, Ninja, Qt 6 for macOS.

Recommended for the full application:

- libarchive: reads zip, 7z, rar and other archive formats through one API.
- Python 3: runs OCR bridge scripts.
- PaddleOCR / manga-ocr Python packages: OCR engines used by the original application.

Optional later:

- Qt Svg: SVG icon rendering if the UI uses SVG assets.
- Qt Concurrent: background scanning/OCR jobs if `QThreadPool` is preferred.
- Catch2 or expanded Qt Test coverage for broader unit tests. The current project uses Qt Test.

Used when available:

- Qt LinguistTools: builds `.qm` translation files from `translations/*.ts`.

## Internationalization

Qt 6's recommended Widgets/C++ workflow is:

- Wrap user-visible strings with `tr()`.
- Use `lupdate` to extract strings into `.ts` files.
- Translate `.ts` files with Qt Linguist.
- Use `lrelease` to compile `.qm` files.
- Load `.qm` files at startup with `QTranslator`.

The project already installs a `QTranslator` before creating the main window and reserves `translations/` for `.ts` files. Keep new UI text inside `tr()` calls so future translation extraction stays mechanical.

## Dependency Choices

Use Qt itself wherever it is good enough:

- JSON: `QJsonDocument`, `QJsonObject`, `QJsonArray`.
- Settings: `QSettings`.
- Processes: `QProcess`.
- Networking and downloads: add Qt Network when OCR model installation is implemented.
- Image display and editing: `QImage`, `QPixmap`, `QGraphicsView`, `QGraphicsScene`.

Add external C/C++ packages only where Qt is weak:

- Archive support: prefer libarchive over a zip-only library because the original app supports zip, 7z and rar.
- OCR runtime: keep OCR engines as Python subprocesses. Do not embed PaddleOCR or manga-ocr into C++; use `QProcess` and JSON/stdout bridges.
- Packaging: use CPack first, then evaluate linuxdeployqt or AppImage tooling when release packaging starts.
