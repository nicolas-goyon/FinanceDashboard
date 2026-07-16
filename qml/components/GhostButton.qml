import QtQuick
import QtQuick.Controls

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
    property color textColor: "#cdd6f4"
    property real fontSize: 13
    property bool bold: false
    property real horizontalPadding: 8

    signal clicked()

    readonly property color restingTextColor: root.variant === "danger" ? "#6c7086" : root.textColor
    readonly property color activeTextColor: root.variant === "danger" ? "#f38ba8" : root.textColor
    readonly property color hoverBg: root.variant === "danger" ? "#3d2028" : "#1e1e2e"
    readonly property color hoverBorderColor: root.variant === "danger" ? "#f38ba8" : "#313244"

    implicitWidth: label.implicitWidth + root.horizontalPadding * 2
    implicitHeight: 22
    radius: 4
    color: hover.hovered ? root.hoverBg : "transparent"
    border.color: hover.hovered ? root.hoverBorderColor : "#313244"

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
