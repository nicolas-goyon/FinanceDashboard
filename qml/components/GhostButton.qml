import QtQuick

// ── GhostButton ──────────────────────────────────────────────────────────────
// Small bordered inline-row action — "✎ Edit", "＋ Subcategory", "＋ Add hash",
// "Transactions", and the "×" remove square. `variant: "neutral"` keeps the
// caller's `textColor` at rest and adds a subtle grey hover tint (used by
// Edit/+Add-style actions, which previously had no hover feedback at all —
// added here for consistency, see ARCHITECTURE.MD § Shared Components).
// `variant: "danger"` mutes the text to grey at rest and flashes red
// (bg/border/text) on hover — matches the "×" remove buttons' original
// behavior exactly.
Rectangle {
    id: root

    property string text: ""
    property string variant: "neutral" // "neutral" | "danger"
    property color textColor: Theme.text
    property real fontSize: 13
    property bool bold: false
    property real horizontalPadding: 8

    signal clicked()

    readonly property color restingTextColor: root.variant === "danger" ? Theme.overlay0 : root.textColor
    readonly property color activeTextColor: root.variant === "danger" ? Theme.red : root.textColor
    readonly property color hoverBg: root.variant === "danger" ? Theme.dangerBg : Theme.base
    readonly property color hoverBorderColor: root.variant === "danger" ? Theme.red : Theme.surface0

    implicitWidth: label.implicitWidth + root.horizontalPadding * 2
    implicitHeight: 22
    radius: 4
    color: hover.hovered ? root.hoverBg : "transparent"
    border.color: hover.hovered ? root.hoverBorderColor : Theme.surface0

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: root.fontSize
        font.bold: root.bold
        color: hover.hovered ? root.activeTextColor : root.restingTextColor
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked() }
}
