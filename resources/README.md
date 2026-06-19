# Resources

This directory is reserved for runtime assets that should ship with the Qt port.

Planned resource groups:

- `ocr/`: Python bridge scripts and model descriptors.
- `icons/`: application and toolbar icons.

Qt translation source files currently live in the top-level `translations/` directory. CMake compiles generated `.qm` files into the application resource system when `Qt6LinguistTools` is available.

