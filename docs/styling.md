# Styling

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

**Controls style: `Basic`.** Set via `QQuickStyle::setStyle("Basic")` in `src/main.cpp`, right
after `QGuiApplication` is constructed and before the QML engine loads anything (per
[QQuickStyle docs](https://doc.qt.io/qt-6/qquickstyle.html) — must run before QML importing
`QtQuick.Controls` is loaded). Requires `Qt6::QuickControls2` (linked in `CMakeLists.txt`).
Native styles (Windows, macOS, ...) reject `background:`/`contentItem:` customization on
Controls — this app customizes `TextField`, `ComboBox`, `Popup`, etc. everywhere, so `Basic`
(Qt's own recommendation for full customization) is required to avoid the "current style does
not support customization" runtime warning/breakage.

Catppuccin Mocha palette, currently hardcoded as hex literals in each `.qml` file (same
approach as QtGraphsExamples). See `TODO.md` for centralizing this in a `Theme.qml` singleton.

| Token | Hex | Used for |
|---|---|---|
| Base | `#11111b` | Window background |
| Mantle | `#181825` | Tab bar background |
| Surface 0 | `#1e1e2e` | Cards, empty-state background |
| Surface 1 | `#313244` | Borders, dividers |
| Overlay 0 | `#6c7086` | Muted labels, inactive tabs |
| Text | `#cdd6f4` | Primary text |
| Blue | `#89b4fa` | Active tab, Stocks layer |
| Green | `#a6e3a1` | ETFs layer, positive P&L |
| Yellow | `#f9e2af` | Metals layer |
| Red/Pink | `#f38ba8` | Coins layer, negative P&L |

Chart fills use the same colors at ~43% opacity (`#6e` alpha prefix).
