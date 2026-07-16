import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One asset-category card of the Entry form (Stocks / Coins / Metals / ETFs).
// All logic lives in the C++ CategoryModel — this file only renders rows.

Rectangle {
    id: root

    property string title: ""
    property string assetType: ""
    property var categoryModel: null

    color: "#1e1e2e"
    radius: 8
    border.color: "#313244"
    implicitHeight: content.implicitHeight + 24

    ColumnLayout {
        id: content
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
        spacing: 8

        // Header: title + running total
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: root.title
                color: "#cdd6f4"
                font { pixelSize: 16; bold: true }
            }
            Item { Layout.fillWidth: true }
            Text {
                text: root.categoryModel ? root.categoryModel.totalText : ""
                color: "#6c7086"
                font { pixelSize: 15; bold: true }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // Dynamic rows
        Repeater {
            model: root.categoryModel

            delegate: RowLayout {
                id: rowDelegate
                Layout.fillWidth: true
                spacing: 8

                required property int index
                required property string name
                required property string valueText
                required property string diffText
                required property string diffState

                TextField {
                    Layout.fillWidth: true
                    text: rowDelegate.name
                    placeholderText: root.assetType + " name"
                    placeholderTextColor: "#6c7086"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    background: Rectangle {
                        color: "#11111b"
                        radius: 4
                        border.color: parent.activeFocus ? "#89b4fa" : "#313244"
                    }
                    onTextEdited: root.categoryModel.setRowName(rowDelegate.index, text)
                }

                TextField {
                    Layout.preferredWidth: 130
                    text: rowDelegate.valueText
                    placeholderText: "0.00"
                    placeholderTextColor: "#6c7086"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    background: Rectangle {
                        color: "#11111b"
                        radius: 4
                        border.color: parent.activeFocus ? "#a6e3a1" : "#313244"
                    }
                    onTextEdited: root.categoryModel.setRowValue(rowDelegate.index, text)
                }

                // Diff badge (green / red / grey — computed in C++)
                Rectangle {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 24
                    radius: 4
                    color: rowDelegate.diffState === "up"   ? "#1a3328"
                         : rowDelegate.diffState === "down" ? "#3d1a28"
                         : "transparent"
                    border.color: rowDelegate.diffState === "up"   ? "#2d5a45"
                                : rowDelegate.diffState === "down" ? "#5a2d3d"
                                : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: rowDelegate.diffText
                        font { pixelSize: 14; bold: rowDelegate.diffState !== "none" }
                        color: rowDelegate.diffState === "up"   ? "#a6e3a1"
                             : rowDelegate.diffState === "down" ? "#f38ba8"
                             : "#6c7086"
                    }
                }

                // Delete button
                Rectangle {
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    radius: 4
                    color: delTap.hovered ? "#3d2028" : "transparent"
                    border.color: delTap.hovered ? "#f38ba8" : "#313244"
                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        font { pixelSize: 17; bold: true }
                        color: delTap.hovered ? "#f38ba8" : "#6c7086"
                    }
                    HoverHandler { id: delTap }
                    TapHandler {
                        onTapped: root.categoryModel.removeRow(rowDelegate.index)
                    }
                }
            }
        }

        // Add button
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: 4
            color: addHover.hovered ? "#181825" : "transparent"
            border.color: "#313244"
            Text {
                anchors.centerIn: parent
                text: "＋  Add " + root.assetType
                font.pixelSize: 15
                color: "#89b4fa"
            }
            HoverHandler { id: addHover }
            TapHandler {
                onTapped: root.categoryModel.addRow()
            }
        }
    }
}
