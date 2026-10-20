pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Updates — own sidebar category in ADMIN, no sub-actions (#117) ───────────
// The update check of UpdateController (a QML_SINGLETON, shared with the
// banner of Main.qml): the version of this app, the channel (the Test app
// chooses Experimental or Preprod; the prod app follows Stable), the check at
// start (on by default, decision of 2026-10-20), "Check now", and the versions
// of the channel, newest first, each with its notes and its release page.
// The download and the installation come in the second part of #117: for now
// a version opens its release page.

Item {
    id: page

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        // The column, and its 20 px above and below.
        contentHeight: pageColumn.implicitHeight + 40
        clip: true

        ColumnLayout {
            id: pageColumn
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            PageHeader {
                title: "Updates"

                StatusBadge {
                    visible: UpdateController.updateAvailable
                    text: "Version " + UpdateController.latestVersion + " available"
                    variant: "success"
                }
            }

            // ── This app ─────────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: appCol.implicitHeight + 50
                title: "This app"

                ColumnLayout {
                    id: appCol
                    anchors.fill: parent
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: Application.displayName + " — version " + UpdateController.currentVersion
                        font.pixelSize: 16
                        font.bold: true
                        color: "#cdd6f4"
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text { text: "Channel"; font.pixelSize: 14; color: "#a6adc8" }
                        ComboBox {
                            id: channelBox
                            visible: UpdateController.testApp
                            Layout.preferredWidth: 300
                            model: UpdateController.channels
                            textRole: "label"
                            valueRole: "id"
                            currentIndex: indexOfValue(UpdateController.channel)
                            onActivated: UpdateController.channel = currentValue
                        }
                        Text {
                            visible: !UpdateController.testApp
                            text: "Stable (the releases)"
                            font.pixelSize: 14
                            color: "#cdd6f4"
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // The text is part of the CheckBox, so a click on it also
                    // toggles the box and saves the setting (toggled()). The
                    // Basic style shows the text in palette.windowText.
                    CheckBox {
                        Layout.fillWidth: true
                        text: "Check for a new version when the app starts"
                        font.pixelSize: 14
                        palette.windowText: "#cdd6f4"
                        leftPadding: 0
                        checked: UpdateController.checkAtStart
                        onToggled: UpdateController.checkAtStart = checked
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        PillButton {
                            text: UpdateController.checking ? "Checking…" : "Check now"
                            variant: "primary"
                            Layout.preferredHeight: 32
                            enabled: !UpdateController.checking
                            onClicked: UpdateController.check()
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            // Plain: an error text can carry a text of the server.
                            textFormat: Text.PlainText
                            text: UpdateController.statusText
                            font.pixelSize: 13
                            color: UpdateController.statusIsError ? "#f38ba8" : "#a6adc8"
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "The check asks GitHub (api.github.com) for the list of releases. GitHub sees "
                            + "the internet address of this computer and the version of this app; the app "
                            + "sends nothing else, and your data never leaves this computer."
                        font.pixelSize: 12
                        color: "#6c7086"
                    }
                }
            }

            // ── Versions of the channel ──────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: versionsCol.implicitHeight + 50
                title: "Versions"

                ColumnLayout {
                    id: versionsCol
                    anchors.fill: parent
                    spacing: 10

                    EmptyState {
                        visible: UpdateController.releases.length === 0
                        message: UpdateController.listRead ? "No version in this channel yet."
                            : UpdateController.checking ? "Reading the list of versions…"
                            : "Check for updates to see the versions."
                    }

                    Repeater {
                        model: UpdateController.releases

                        delegate: ColumnLayout {
                            id: versionRow
                            required property var modelData
                            property bool notesOpen: false

                            Layout.fillWidth: true
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: versionRow.modelData.version
                                    font.pixelSize: 15
                                    font.bold: true
                                    color: "#cdd6f4"
                                }
                                StatusBadge {
                                    visible: versionRow.modelData.relation !== ""
                                    pillHeight: 20
                                    text: versionRow.modelData.relation === "newer" ? "newer"
                                        : versionRow.modelData.relation === "current" ? "this version" : "older"
                                    variant: versionRow.modelData.relation === "newer" ? "success"
                                        : versionRow.modelData.relation === "current" ? "warning" : "neutral"
                                }
                                Text {
                                    text: versionRow.modelData.date
                                    font.pixelSize: 13
                                    color: "#6c7086"
                                }
                                Item { Layout.fillWidth: true }
                                GhostButton {
                                    visible: versionRow.modelData.notes.length > 0
                                    text: versionRow.notesOpen ? "Hide the notes" : "Notes"
                                    onClicked: versionRow.notesOpen = !versionRow.notesOpen
                                }
                                GhostButton {
                                    text: "Release page"
                                    onClicked: UpdateController.openPage(versionRow.modelData.page)
                                }
                            }

                            Text {
                                visible: versionRow.notesOpen
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                textFormat: Text.MarkdownText
                                text: versionRow.modelData.notes
                                font.pixelSize: 13
                                color: "#a6adc8"
                                onLinkActivated: link => UpdateController.openPage(link)
                            }

                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#313244" }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
