import QtQuick
import QtQuick.Layouts

// Titled card that hosts a chart (or any content) via the default property.
Rectangle {
    id: root

    property string title: ""
    default property alias content: contentHost.data

    color: Theme.base
    radius: 8
    border.color: Theme.surface0
    implicitHeight: 340

    ColumnLayout {
        anchors { fill: parent; margins: 12 }
        spacing: 6

        Text {
            visible: root.title.length > 0
            text: root.title
            font.pixelSize: 16
            font.bold: true
            color: Theme.text
        }

        Item {
            id: contentHost
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
