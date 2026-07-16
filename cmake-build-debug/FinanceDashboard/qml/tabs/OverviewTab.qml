import QtQuick
import QtQuick.Layouts
import FinanceDashboard

// ── Overview tab ──────────────────────────────────────────────────────────────
// Stat cards + top movers. Data comes pre-formatted from OverviewController
// (still debug samples — see ARCHITECTURE.MD).

Item {
    OverviewController { id: controller }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Text {
            text: "Portfolio Overview"
            font { pixelSize: 20; bold: true }
            color: "#cdd6f4"
        }
        Text {
            text: "[DEBUG DATA — not live]"
            font.pixelSize: 11
            color: "#f38ba8"
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── Stat cards (2×3) ──────────────────────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            rowSpacing: 12
            columnSpacing: 12

            Repeater {
                model: controller.cards
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    color: "#1e1e2e"
                    radius: 8

                    ColumnLayout {
                        anchors { fill: parent; leftMargin: 14; rightMargin: 14; topMargin: 10; bottomMargin: 10 }
                        spacing: 2
                        Text {
                            text: modelData.label.toUpperCase()
                            font { pixelSize: 11; bold: true }
                            color: "#6c7086"
                        }
                        Text {
                            text: modelData.value
                            font { pixelSize: 20; bold: true }
                            color: modelData.color
                        }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── Top movers ────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: moversCol.implicitHeight + 24
            color: "transparent"
            border.color: "#313244"
            radius: 6

            ColumnLayout {
                id: moversCol
                anchors { fill: parent; margins: 12 }
                spacing: 6

                Text {
                    text: "Top Movers  [debug]"
                    font { pixelSize: 13; bold: true }
                    color: "#cdd6f4"
                }
                Repeater {
                    model: controller.movers
                    delegate: Text {
                        required property var modelData
                        text: modelData.text
                        font { family: "Consolas"; pixelSize: 13 }
                        color: modelData.up ? "#a6e3a1" : "#f38ba8"
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
