pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── StartupErrorWindow ───────────────────────────────────────────────────────
// Shown by main.cpp instead of Main.qml when a fatal start-up step failed
// (src/startup/StartupChecks — see docs/startup.md). Rendering only: lists
// every start-up step as met / not met with its detail and, for failures, the
// fix hint. `steps` is set by main.cpp via setInitialProperties() as a list of
// { name, ok, fatal, detail, hint } maps (StartupChecks::toVariantList()).
// Catppuccin Mocha palette, same tokens as docs/styling.md.
ApplicationWindow {
    id: root

    required property var steps

    visible: true
    width: 720
    height: 480
    title: "Finance Dashboard — can't start"
    color: "#11111b"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Text {
            Layout.fillWidth: true
            text: "Finance Dashboard can't start"
            color: "#f38ba8"
            font.pixelSize: 22
            font.bold: true
        }
        Text {
            Layout.fillWidth: true
            text: "One or more requirements below are not met. Fix the ones marked ✘ and start the app again."
            color: "#cdd6f4"
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: root.width - 48
                spacing: 8

                Repeater {
                    model: root.steps

                    delegate: Rectangle {
                        id: row
                        required property var modelData

                        Layout.fillWidth: true
                        implicitHeight: rowContent.implicitHeight + 20
                        color: "#1e1e2e"
                        radius: 6
                        border.color: row.modelData.ok ? "#313244" : "#f38ba8"

                        RowLayout {
                            id: rowContent
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 12

                            Text {
                                Layout.alignment: Qt.AlignTop
                                text: row.modelData.ok ? "✔" : "✘"
                                color: row.modelData.ok ? "#a6e3a1" : "#f38ba8"
                                font.pixelSize: 18
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: row.modelData.name
                                          + (!row.modelData.ok && !row.modelData.fatal ? "  (warning)" : "")
                                    color: "#cdd6f4"
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: row.modelData.detail
                                    color: "#6c7086"
                                    font.pixelSize: 13
                                    wrapMode: Text.WrapAnywhere
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: !row.modelData.ok && row.modelData.hint !== ""
                                    text: "Fix: " + row.modelData.hint
                                    color: "#f9e2af"
                                    font.pixelSize: 13
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }

        Button {
            Layout.alignment: Qt.AlignRight
            text: "Quit"
            onClicked: Qt.quit()
        }
    }
}
