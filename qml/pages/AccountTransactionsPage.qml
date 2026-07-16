import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Account Transactions — reached from Manage Accounts → "Transactions" ─────
// Read-only table of every persisted transaction for one account (see
// AccountController::transactionsForAccount() / db/tables/TransactionsTable.h)
// — the "button to access a table with all transactions registered at that
// account" from the product request. A full routed page (not a modal/Popup)
// per product decision — AccountsManagePage.qml's "Transactions" button
// emits viewTransactionsRequested(accountIndex), Main.qml swaps to this
// overlay and sets accountIndex here, closeRequested() returns to Manage
// Accounts (see ARCHITECTURE.MD § Account Management / § Sidebar Navigation).

Item {
    id: page

    signal closeRequested()

    // Set by Main.qml right before showing this overlay — index into
    // AccountController.accounts. -1 (or out of range) shows an empty state
    // instead of erroring, e.g. if the account list changed underneath.
    property int accountIndex: -1

    readonly property var account:
        page.accountIndex >= 0 && page.accountIndex < AccountController.accounts.length
            ? AccountController.accounts[page.accountIndex] : null

    readonly property var rows:
        page.accountIndex >= 0 ? AccountController.transactionsForAccount(page.accountIndex) : []

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            // ── Header ────────────────────────────────────────────────────────
            RowLayout {
                spacing: 12
                Rectangle {
                    width: 26; height: 26; radius: 4
                    color: backHover.hovered ? "#181825" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "←"
                        font.pixelSize: 19
                        color: "#89b4fa"
                    }
                    HoverHandler { id: backHover }
                    TapHandler { onTapped: page.closeRequested() }
                }
                Text {
                    text: "Transactions" + (page.account !== null ? " — " + page.account.name : "")
                    font.pixelSize: 23
                    font.bold: true
                    color: "#cdd6f4"
                }
                Item { Layout.fillWidth: true }
                Text { text: page.rows.length + " row(s)"; font.pixelSize: 14; color: "#6c7086" }
            }

            Text {
                visible: page.account === null
                text: "This account is no longer available."
                font.pixelSize: 15
                color: "#f38ba8"
            }

            // ── Table ─────────────────────────────────────────────────────────
            ChartCard {
                visible: page.account !== null
                Layout.fillWidth: true
                implicitHeight: txCol.implicitHeight + 50
                title: "All transactions"

                ColumnLayout {
                    id: txCol
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        color: "#181825"
                        radius: 4
                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            spacing: 8
                            Text { Layout.preferredWidth: 90;  text: "DATE";        font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            Text { Layout.fillWidth: true;     text: "DESCRIPTION"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            Text { Layout.preferredWidth: 90;  text: "DEBIT";       font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 90;  text: "CREDIT";      font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 100; text: "BALANCE";     font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 120; text: "CATEGORY";    font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                        }
                    }

                    Text {
                        visible: page.rows.length === 0
                        Layout.topMargin: 10
                        text: "No transactions saved for this account yet."
                        font.pixelSize: 15
                        color: "#6c7086"
                    }

                    Repeater {
                        model: page.rows
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            color: index % 2 === 0 ? "transparent" : "#181825"

                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                spacing: 8
                                Text { Layout.preferredWidth: 90; text: modelData.operationDate; font.pixelSize: 14; color: "#6c7086" }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.description
                                    font.pixelSize: 15
                                    color: "#cdd6f4"
                                    elide: Text.ElideRight
                                }
                                Text { Layout.preferredWidth: 90;  text: modelData.debitText;  font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 90;  text: modelData.creditText; font.pixelSize: 15; color: "#a6e3a1"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 100; text: modelData.balanceText; font.pixelSize: 14; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 120; text: modelData.category.length > 0 ? modelData.category : "—"; font.pixelSize: 14; color: "#6c7086" }
                            }
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }
}
