import QtQuick
import QtQuick.Layouts

// Titled card that hosts a chart (or any content) via the default property.
Rectangle {
    id: root

    property string title: ""
    default property alias content: contentHost.data

    color: "#1e1e2e"
    radius: 8
    border.color: "#313244"
    implicitHeight: 340

    ColumnLayout {
        anchors { fill: parent; margins: 12 }
        spacing: 6

        Text {
            visible: root.title.length > 0
            text: root.title
            font.pixelSize: 16
            font.bold: true
            color: "#cdd6f4"
        }

        Item {
            id: contentHost
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
