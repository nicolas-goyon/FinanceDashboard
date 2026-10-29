pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// One collapsible sidebar entry.
// - Clicking the label (or the row background) selects the category's page.
// - Clicking the small arrow expands/collapses the action list without
//   changing the selected view.
// - `current` and `currentActionId` are computed by the parent (Main.qml)
//   from its single `currentViewId` source of truth and simply handed down —
//   this component renders what it's told, it doesn't know about routing.
//   `current` covers both "this category's own page is shown" and "one of
//   this category's actions/overlays is shown", so the header stays
//   highlighted the whole time you're anywhere inside the category — see
//   ARCHITECTURE.MD § Sidebar Navigation.
Rectangle {
    id: root

    property string categoryLabel: ""
    property var actions: []      // [{ id, label }, ...]
    property bool current: false
    property string currentActionId: ""
    property bool expanded: false

    signal selected()
    signal actionTriggered(string actionId)

    color: "transparent"
    implicitHeight: headerRow.height + (expanded ? actionsColumn.implicitHeight : 0)

    Behavior on implicitHeight { NumberAnimation { duration: 120 } }

    // Auto-expand the action list the first time this category becomes
    // active, so navigating here (e.g. via the "Transactions" button on an
    // account row, which activates Account Management without the user ever
    // clicking its arrow) always reveals which action put you here. Doesn't
    // fight the user afterward — they can still collapse/expand freely once
    // shown.
    onCurrentChanged: if (root.current) root.expanded = true

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: 0

        // ── Header row: label (selects page) + arrow (expands) ─────────────
        Rectangle {
            id: headerRow
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: root.current ? Theme.surface0 : (labelArea.containsMouse ? Theme.base : "transparent")

            Rectangle {
                visible: root.current
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: 3
                color: Theme.blue
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
                        color: root.current ? Theme.blue : Theme.text
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
                        color: Theme.overlay0
                        font.pixelSize: 15
                    }
                }
            }
        }

        // ── Actions list — highlights whichever action is currently active
        //    the same way the header does (accent bar + bold + blue text),
        //    so drilling into an overlay page shows exactly where you are,
        //    not just which category you're in. ────────────────────────────
        Column {
            id: actionsColumn
            Layout.fillWidth: true
            visible: root.expanded
            clip: true

            Repeater {
                model: root.actions
                delegate: Rectangle {
                    id: actionRow
                    required property var modelData
                    readonly property bool isActive: modelData.id === root.currentActionId
                    width: actionsColumn.width
                    height: 30
                    color: actionRow.isActive ? Theme.surfaceHover : (actionMouse.containsMouse ? Theme.base : "transparent")

                    Rectangle {
                        visible: actionRow.isActive
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                        width: 3
                        color: Theme.blue
                    }

                    Text {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                        anchors.leftMargin: 36
                        text: actionRow.modelData.label
                        color: actionRow.isActive ? Theme.blue : Theme.subtext0
                        font.pixelSize: 15
                        font.bold: actionRow.isActive
                    }

                    MouseArea {
                        id: actionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.actionTriggered(actionRow.modelData.id)
                    }
                }
            }
        }
    }
}
