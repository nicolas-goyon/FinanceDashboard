pragma Singleton

import QtQuick

// ── Theme ────────────────────────────────────────────────────────────────────
// Every colour of the QML, in one place (#79). Use `Theme.<name>`. Do not
// write a hex colour in a page or a component.
//
// The palette is Catppuccin Mocha (https://catppuccin.com/palette). The names
// of the first block are the names of that palette. The second block has the
// colours that are not in the palette, named by their use.
//
// The colours that come from C++ (categories, accounts, KPI values) are not
// here: the controllers give them as data.
QtObject {
    // ── Catppuccin Mocha ─────────────────────────────────────────────────────
    readonly property color crust: "#11111b"     // window background, input fields
    readonly property color mantle: "#181825"    // sidebar, plot areas, tooltips
    readonly property color base: "#1e1e2e"      // cards
    readonly property color surface0: "#313244"  // borders, grid lines
    readonly property color surface1: "#45475a"  // stronger borders, placeholders
    readonly property color overlay0: "#6c7086"  // secondary text, labels
    readonly property color subtext0: "#a6adc8"  // chips
    readonly property color text: "#cdd6f4"      // main text
    readonly property color blue: "#89b4fa"
    readonly property color green: "#a6e3a1"
    readonly property color yellow: "#f9e2af"
    readonly property color peach: "#fab387"
    readonly property color red: "#f38ba8"
    readonly property color mauve: "#cba6f7"

    // ── Outside the palette ──────────────────────────────────────────────────
    readonly property color surfaceHover: "#232336"     // a hovered card or sidebar action
    readonly property color primaryBg: "#1e3a5f"        // primary button, update banner
    readonly property color primaryBgHover: "#264d80"
    readonly property color successBg: "#1a3328"        // status badges
    readonly property color successBgStrong: "#2d5a45"
    readonly property color warningBg: "#3d3320"
    readonly property color warningBgStrong: "#5a4d2d"
    readonly property color dangerBg: "#3d2028"
    readonly property color dangerBgStrong: "#5a2d36"
    readonly property color dangerBannerBg: "#4c2032"   // failed update banner

    // ── Area fills (palette colour, alpha 0x6e) ──────────────────────────────
    readonly property color blueArea: "#6e89b4fa"
    readonly property color greenArea: "#6ea6e3a1"
    readonly property color mauveArea: "#6ecba6f7"
}
