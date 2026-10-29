import QtQuick
import QtQuick.Layouts

// Small stat card: LABEL / value / optional sub-line.
Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property string sub: ""
    property color valueColor: Theme.text

    color: Theme.base
    radius: 8
    implicitHeight: col.implicitHeight + 20

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 2

        Text {
            text: root.label.toUpperCase()
            font.pixelSize: 13
            font.bold: true
            color: Theme.overlay0
        }
        Text {
            text: root.value
            font.pixelSize: 22
            font.bold: true
            color: root.valueColor
        }
        Text {
            visible: root.sub.length > 0
            text: root.sub
            font.pixelSize: 13
            color: Theme.overlay0
        }
    }
}
