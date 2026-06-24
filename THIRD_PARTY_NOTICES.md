# Third-Party Notices

This file summarizes third-party code and assets bundled in this repository.

## BreezeStyleSheets

- Source: <https://github.com/Alexhuszagh/BreezeStyleSheets>
- Bundled path: `resources/themes/breeze`
- License: MIT
- Local license file: `resources/themes/breeze/LICENSE.md`

The bundled stylesheets provide Breeze-like Qt Widgets themes.

## Material UI Icons

- Bundled path: `resources/themes/breeze`
- License: Apache License 2.0
- Local license file: `resources/themes/breeze/MaterialUi.LICENSE`

Some SVG assets used by BreezeStyleSheets are derived from Material UI / Material Design icon sources.

## QtKeychain

- Source: <https://github.com/frankosterfeld/qtkeychain>
- Usage: linked as a system/library dependency to store automation script secrets in the operating system keychain.
- License: BSD-style license. See the QtKeychain project for the exact license text shipped by the installed package.

## External APIs Not Bundled

The official automation scripts may call user-configured external services, such as the DeepSeek API for AI
translation. These services, API keys, hosted models and remote runtimes are not bundled in this repository. Users are
responsible for configuring credentials and complying with the relevant service terms.
