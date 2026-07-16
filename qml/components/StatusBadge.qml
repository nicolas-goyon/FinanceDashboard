import QtQuick

// ── StatusBadge ──────────────────────────────────────────────────────────────
// Colored pill used for every status/state indicator in the app: the "[MOCK
// DATA]"-adjacent success pill on Import/Database Management, the
// linked/not-linked badge, the parse issue-count and log-level badges, and
// the project on-track/gap badge. Before this was extracted there were 7+
// hand-rolled versions with drifting radius/height and two real color bugs
// (ProjectsPage's off-track background was "#3d1a28", a transposed digit —
// every other danger badge in the app uses "#3d2028" — and its Rectangle had
// no border at all). Both are fixed by routing everything through one
// component — see the component-duplication survey in ARCHITECTURE.MD §
// Shared Components.
//
// `pillHeight` stays a caller-set property (not a fixed size) because the
// app genuinely uses this at several sizes on purpose — 26px status pills,
// 22-24px inline chips, 18px log-entry badges — radius is always half the
// height so it's always a true pill regardless of size.
Rectangle {
    id: root

    property string text: ""
    property string variant: "neutral" // "success" | "warning" | "danger" | "neutral"
    property real pillHeight: 24
    property real horizontalPadding: 10

    implicitWidth: label.implicitWidth + root.horizontalPadding * 2
    implicitHeight: root.pillHeight
    radius: root.pillHeight / 2

    color: {
        if (root.variant === "success") return "#1a3328"
        if (root.variant === "warning") return "#3d3320"
        if (root.variant === "danger")  return "#3d2028"
        return "#181825" // neutral
    }
    border.color: {
        if (root.variant === "success") return "#2d5a45"
        if (root.variant === "warning") return "#5a4d2d"
        if (root.variant === "danger")  return "#5a2d36"
        return "#313244" // neutral
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: root.pillHeight >= 24 ? 14 : 12
        color: {
            if (root.variant === "success") return "#a6e3a1"
            if (root.variant === "warning") return "#f9e2af"
            if (root.variant === "danger")  return "#f38ba8"
            return "#a6adc8" // neutral
        }
    }
}
