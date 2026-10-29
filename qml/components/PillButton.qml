import QtQuick

// ── PillButton ───────────────────────────────────────────────────────────────
// The big colored action button used everywhere (Add/Save/Cancel/Delete in
// dialogs, "+ Add X" header buttons, Database Management's trigger buttons,
// Import's "Choose file"/"Import"/"Link this account"). Three color
// families, matched exactly to what every page already used before this was
// extracted (see the component-duplication survey in ARCHITECTURE.MD §
// Shared Components):
//   primary — blue border #89b4fa, bg #1e3a5f / hover #264d80, text #cdd6f4
//   neutral — grey border #313244, bg #1e1e2e / hover #313244, text #cdd6f4
//   danger  — red border #f38ba8, bg #3d2028 / hover #5a2d36, text #f38ba8
// Width is implicit (label + padding) unless the caller sets an explicit
// width/height — every page previously hand-picked its own pixel width for
// what was conceptually the same button; auto-sizing removes that drift.
Rectangle {
    id: root

    property string text: ""
    property string variant: "primary" // "primary" | "neutral" | "danger"
    property bool bold: true
    property real horizontalPadding: 16
    property real fontSize: 15
    // NOTE: `enabled` is Item's own built-in property (not redeclared here) —
    // setting it on an instance dims + disables input the normal QtQuick way.

    signal clicked()

    implicitWidth: label.implicitWidth + root.horizontalPadding * 2
    implicitHeight: 30
    radius: 4
    opacity: root.enabled ? 1.0 : 0.5

    color: {
        if (root.variant === "danger")
            return hover.hovered ? Theme.dangerBgStrong : Theme.dangerBg
        if (root.variant === "neutral")
            return hover.hovered ? Theme.surface0 : Theme.base
        return hover.hovered ? Theme.primaryBgHover : Theme.primaryBg // primary
    }
    border.color: {
        if (root.variant === "danger")  return Theme.red
        if (root.variant === "neutral") return Theme.surface0
        return Theme.blue // primary
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: root.fontSize
        font.bold: root.bold
        color: root.variant === "danger" ? Theme.red : Theme.text
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked() }
}
