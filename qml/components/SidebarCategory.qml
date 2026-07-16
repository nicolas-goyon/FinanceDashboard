import QtQuick
import QtQuick.Layouts

// One collapsible sidebar entry.
// - Clicking the label (or the row background) selects the category's page.
// - Clicking the small arrow expands/collapses the action list without
//   changing the selected page.
// - Actions are pre-filled labels; most are still placeholders (no-op), but
//   an action click always emits actionTriggered() so a parent can wire up
//   specific ones (see Main.qml: Data Entry → "New Monthly Snapshot").
Rectangle {
    id: root

    property string categoryLabel: ""
    property var actions: []
    property bool current: false
    property bool expanded: false

    signal selected()
    signal actionTriggered(string action)

    color: "transparent"
    implicitHeight: headerRow.height + (expanded ? actionsColumn.implicitHeight : 0)

    Behavior on implicitHeight { NumberAnimation { duration: 120 } }

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: 0

        // ── Header row: label (selects page) + arrow (expands) ─────────────
        Rectangle {
            id: headerRow
            Layout.fillWidth: true
            height: 40
            color: root.current ? "#313244" : (labelArea.containsMouse ? "#1e1e2e" : "transparent")

            Rectangle {
                visible: root.current
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: 3
                color: "#89b4fa"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 4
                spacing: 4

                MouseArea {
                    id: labelArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selected()

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.categoryLabel
                        color: root.current ? "#89b4fa" : "#cdd6f4"
                        font { pixelSize: 17; bold: root.current }
                    }
                }

                MouseArea {
                    id: arrowArea
                    Layout.preferredWidth: 28
                    Layout.fillHeight: true
                    visible: root.actions.length > 0
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.expanded = !root.expanded

                    Text {
                        anchors.centerIn: parent
                        text: root.expanded ? "▾" : "▸" // ▾ / ▸
                        color: "#6c7086"
                        font.pixelSize: 15
                    }
                }
            }
        }

        // ── Actions list (pre-filled labels, no navigation yet) ────────────
        Column {
            id: actionsColumn
            Layout.fillWidth: true
            visible: root.expanded
            clip: true

            Repeater {
                model: root.actions
                delegate: Rectangle {
                    required property string modelData
                    width: actionsColumn.width
                    height: 30
                    color: actionMouse.containsMouse ? "#1e1e2e" : "transparent"

                    Text {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                        anchors.leftMargin: 36
                        text: parent.modelData
                        color: "#a6adc8"
                        font.pixelSize: 15
                    }

                    MouseArea {
                        id: actionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.actionTriggered(parent.modelData)
                    }
                }
            }
        }
    }
}
