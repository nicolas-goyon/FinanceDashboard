import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FinanceDashboard

// ── Import tab ────────────────────────────────────────────────────────────────
// Load a portfolio JSON file and preview its contents.
// Parsing + formatting happen in ImportController (JsonReader).

Item {
    ImportController { id: controller }

    FileDialog {
        id: fileDialog
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        onAccepted: pathField.text = controller.localPath(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Text {
            text: "Import Data"
            font { pixelSize: 23; bold: true }
            color: "#cdd6f4"
        }
        Text {
            text: "Load a portfolio JSON file to preview its contents."
            font.pixelSize: 15
            color: "#6c7086"
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── File picker row ───────────────────────────────────────────────────
        RowLayout {
            spacing: 8

            Text { text: "File path:"; font.pixelSize: 15; color: "#6c7086" }

            TextField {
                id: pathField
                Layout.fillWidth: true
                placeholderText: "C:/data/portfolio.json"
                placeholderTextColor: "#6c7086"
                color: "#cdd6f4"
                font.pixelSize: 15
                background: Rectangle {
                    color: "#1e1e2e"
                    radius: 4
                    border.color: pathField.activeFocus ? "#89b4fa" : "#313244"
                }
            }

            Rectangle {
                width: 90; height: 30; radius: 4
                color: browseHover.hovered ? "#313244" : "#1e1e2e"
                border.color: "#313244"
                Text { anchors.centerIn: parent; text: "Browse…"; font.pixelSize: 15; color: "#cdd6f4" }
                HoverHandler { id: browseHover }
                TapHandler { onTapped: fileDialog.open() }
            }
            Rectangle {
                width: 90; height: 30; radius: 4
                color: importHover.hovered ? "#264d80" : "#1e3a5f"
                border.color: "#89b4fa"
                Text { anchors.centerIn: parent; text: "Import"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                HoverHandler { id: importHover }
                TapHandler { onTapped: controller.importFile(pathField.text) }
            }
        }

        // ── Status ────────────────────────────────────────────────────────────
        Text {
            text: controller.statusText
            font.pixelSize: 14
            color: controller.statusState === "ok"    ? "#a6e3a1"
                 : controller.statusState === "error" ? "#f38ba8"
                 : "#6c7086"
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── Results ───────────────────────────────────────────────────────────

        // Hint / error message (no result)
        Text {
            visible: !controller.hasResult
            Layout.fillWidth: true
            Layout.topMargin: 20
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: controller.message
            font.pixelSize: 16
            color: controller.statusState === "error" ? "#f38ba8" : "#6c7086"
        }
        Item { visible: !controller.hasResult; Layout.fillHeight: true }

        ScrollView {
            visible: controller.hasResult
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 20

                // ── Summary card ──────────────────────────────────────────────
                Text {
                    text: "Portfolio Summary"
                    font { pixelSize: 16; bold: true }
                    color: "#89b4fa"
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: summaryGrid.implicitHeight + 20
                    color: "#1e1e2e"
                    radius: 8

                    GridLayout {
                        id: summaryGrid
                        anchors { fill: parent; margins: 10; leftMargin: 14; rightMargin: 14 }
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 8

                        // Flatten [{key, value, color}] into alternating
                        // key/value cells for the 2-column GridLayout.
                        Repeater {
                            model: {
                                let flat = []
                                for (let i = 0; i < controller.summaryRows.length; i++) {
                                    const r = controller.summaryRows[i]
                                    flat.push({ text: r.key,   color: "#6c7086", bold: false, size: 15 })
                                    flat.push({ text: r.value, color: r.color,   bold: true,  size: 16 })
                                }
                                return flat
                            }
                            delegate: Text {
                                required property var modelData
                                text: modelData.text
                                font { pixelSize: modelData.size; bold: modelData.bold }
                                color: modelData.color
                            }
                        }
                    }
                }

                // ── Time series ───────────────────────────────────────────────
                Text {
                    text: "Time Series  (" + controller.totalPoints
                          + (controller.totalPoints === 1 ? " point)" : " points)")
                    font { pixelSize: 16; bold: true }
                    color: "#89b4fa"
                }

                Text {
                    visible: controller.totalPoints === 0
                    text: "No data points in this file."
                    font.pixelSize: 15
                    color: "#6c7086"
                }

                Rectangle {
                    visible: controller.totalPoints > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(60 + controller.points.length * 28, 400)
                    color: "#1e1e2e"
                    radius: 6
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            color: "#181825"
                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                Text { Layout.fillWidth: true; text: "Timestamp"; font.pixelSize: 14; font.bold: true; color: "#6c7086" }
                                Text { text: "Value"; font.pixelSize: 14; font.bold: true; color: "#6c7086" }
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: controller.points
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                width: ListView.view.width
                                height: 28
                                color: index % 2 === 0 ? "#1e1e2e" : "#181825"
                                RowLayout {
                                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                    Text { Layout.fillWidth: true; text: modelData.time; font.pixelSize: 15; color: "#cdd6f4"; elide: Text.ElideRight }
                                    Text { text: modelData.value; font.pixelSize: 15; color: "#cdd6f4" }
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: controller.capNote.length > 0
                    text: controller.capNote
                    font.pixelSize: 14
                    color: "#6c7086"
                }
            }
        }
    }
}
