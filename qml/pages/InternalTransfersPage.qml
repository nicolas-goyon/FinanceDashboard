pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Internal Transfers — Transactions → "Internal Transfers" (#207) ─────────
// A move of money between two accounts of the net worth (checking → Livret A)
// is not income and not spending. This page shows the pairs that the app
// finds (a debit and a credit of the same amount on two accounts of the net
// worth, 3 days apart or less), and the confirmed transfers.
// Backed by TransfersController (src/controllers/dataentry/
// TransfersController.h). A single row is marked by hand on the
// transactions page ("✎ Edit" → "Internal transfer").
Item {
    id: page

    signal closeRequested()

    TransfersController { id: transfers }

    Component.onCompleted: transfers.refresh()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            PageHeader {
                showBack: true
                title: "Internal Transfers"
                onBackClicked: page.closeRequested()

                StatusBadge {
                    visible: transfers.statusText.length > 0
                    text: transfers.statusText
                    variant: transfers.statusLevel
                }
                PillButton {
                    visible: transfers.suggestions.length > 0
                    text: "Confirm all (" + transfers.suggestions.length + ")"
                    variant: "primary"
                    onClicked: transfers.confirmAll()
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "A move of money between two accounts of your net worth (for example from the checking "
                    + "account to the Livret A) is not income and not spending. The app suggests a debit and a "
                    + "credit of the same amount on two accounts of your net worth, 3 days apart or less. "
                    + "Confirm or refuse each one. To choose the accounts of your net worth, go to Manage Accounts."
                font.pixelSize: 14
                color: Theme.overlay0
            }

            // ── Suggestions ──────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: suggestionsCol.implicitHeight + 50
                title: "Suggestions (" + transfers.suggestions.length + ")"

                ColumnLayout {
                    id: suggestionsCol
                    anchors.fill: parent
                    spacing: 6

                    EmptyState {
                        visible: transfers.suggestions.length === 0
                        message: "No transfer to confirm."
                    }

                    Repeater {
                        model: transfers.suggestions
                        delegate: Rectangle {
                            id: suggestionRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            color: index % 2 === 0 ? "transparent" : Theme.mantle

                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                spacing: 10
                                Text { Layout.preferredWidth: 90; text: suggestionRow.modelData.dateText; font.pixelSize: 14; color: Theme.overlay0 }
                                Text {
                                    Layout.preferredWidth: 110
                                    text: suggestionRow.modelData.amountText
                                    font.pixelSize: 15
                                    font.bold: true
                                    color: Theme.text
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: suggestionRow.modelData.fromAccount + "  →  " + suggestionRow.modelData.toAccount
                                          + "    (" + suggestionRow.modelData.fromName + " / " + suggestionRow.modelData.toName + ")"
                                    font.pixelSize: 14
                                    color: Theme.text
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: suggestionRow.modelData.days > 0
                                    text: suggestionRow.modelData.days + " day(s) apart"
                                    font.pixelSize: 12
                                    color: Theme.overlay0
                                }
                                GhostButton {
                                    text: "✓ Confirm"
                                    textColor: Theme.green
                                    onClicked: transfers.confirm(suggestionRow.index)
                                }
                                GhostButton {
                                    text: "Not a transfer"
                                    variant: "danger"
                                    onClicked: transfers.refuse(suggestionRow.index)
                                }
                            }
                        }
                    }
                }
            }

            // ── Confirmed transfers ──────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: transfersCol.implicitHeight + 50
                title: "Confirmed transfers (" + transfers.transfers.length + ")"

                ColumnLayout {
                    id: transfersCol
                    anchors.fill: parent
                    spacing: 6

                    EmptyState {
                        visible: transfers.transfers.length === 0
                        message: "No confirmed transfer yet."
                    }

                    Repeater {
                        model: transfers.transfers
                        delegate: Rectangle {
                            id: transferRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            color: index % 2 === 0 ? "transparent" : Theme.mantle

                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                spacing: 10
                                Text { Layout.preferredWidth: 90; text: transferRow.modelData.dateText; font.pixelSize: 14; color: Theme.overlay0 }
                                Text {
                                    Layout.preferredWidth: 110
                                    text: transferRow.modelData.amountText
                                    font.pixelSize: 15
                                    color: Theme.text
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: transferRow.modelData.fromAccount + "  →  " + transferRow.modelData.toAccount
                                          + "    " + transferRow.modelData.name
                                    font.pixelSize: 14
                                    color: Theme.text
                                    elide: Text.ElideRight
                                }
                                StatusBadge {
                                    visible: !transferRow.modelData.complete
                                    variant: "warning"
                                    pillHeight: 20
                                    text: "one side only"
                                }
                                GhostButton {
                                    text: "Cancel"
                                    variant: "danger"
                                    onClicked: transfers.cancelTransfer(transferRow.modelData.transferId)
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
