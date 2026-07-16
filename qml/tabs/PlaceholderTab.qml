import QtQuick

// Temporary stand-in for tabs not yet ported from Qt Widgets to QML.
// See ARCHITECTURE.MD § Migration Status.

Item {
    property string tabName: ""

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 520)
        height: 120
        color: "#1e1e2e"
        border.color: "#313244"
        radius: 8

        Text {
            anchors.centerIn: parent
            text: tabName + " — not yet migrated to QML.\nThe Widgets implementation is kept in src/tabs/ as reference."
            horizontalAlignment: Text.AlignHCenter
            color: "#6c7086"
            font.pixelSize: 16
        }
    }
}
