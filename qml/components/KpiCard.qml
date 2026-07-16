import QtQuick
import QtQuick.Layouts

// Small stat card: LABEL / value / optional sub-line.
Rectangle {
    property string label: ""
    property string value: ""
    property string sub: ""
    property color valueColor: "#cdd6f4"

    color: "#1e1e2e"
    radius: 8
    implicitHeight: col.implicitHeight + 20

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 2

        Text {
            text: label.toUpperCase()
            font.pixelSize: 13
            font.bold: true
            color: "#6c7086"
        }
        Text {
            text: value
            font.pixelSize: 22
            font.bold: true
            color: valueColor
        }
        Text {
            visible: sub.length > 0
            text: sub
            font.pixelSize: 13
            color: "#6c7086"
        }
    }
}
