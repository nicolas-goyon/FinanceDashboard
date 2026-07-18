import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Database Management — own sidebar category, no sub-actions ───────────────
// Lets the developer/user wipe the SQLite file back to a blank slate
// ("Reset Database") or wipe-then-reseed a small demo dataset ("Reset to
// Mock Data") — useful whenever the schema changes during development (see
// ARCHITECTURE.MD § Database Management). Backed by
// DatabaseManagementController (one per page, not a QML_SINGLETON — no
// state to keep alive). Both actions are destructive and irreversible, so
// both are gated behind a confirmation dialog first — both dialogs use the
// red/danger confirm styling regardless of the trigger button's own color
// (Reset to Mock Data's trigger is blue/primary, its confirm step is still
// red, deliberately, since it's just as irreversible as a full reset).
//
// After either action, CategoryController.reload() and
// AccountController.reload() are called here (not by the controller itself
// — see DatabaseManagementController's class comment) so those singletons'
// cached lists pick up the wipe/reseed immediately instead of only on next
// app launch.
//
// Built from the shared qml/components/ (PageHeader, StatusBadge,
// PillButton, ConfirmDialog) — see ARCHITECTURE.MD § Shared Components.

Item {
    id: page

    DatabaseManagementController {
        id: controller
        onDatabaseReset: {
            CategoryController.reload()
            AccountController.reload()
        }
    }

    ColumnLayout {
        x: 20; y: 20
        width: parent.width - 40
        spacing: 16

        PageHeader {
            title: "Database Management"

            StatusBadge {
                visible: controller.statusText.length > 0
                text: controller.statusText
                variant: "success"
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Both actions wipe every table in the local SQLite database. Useful when the schema "
                + "changes during development, or to start over. Neither action can be undone."
            font.pixelSize: 15
            color: "#6c7086"
        }

        ChartCard {
            Layout.fillWidth: true
            implicitHeight: actionsCol.implicitHeight + 50
            title: "Reset actions"

            ColumnLayout {
                id: actionsCol
                anchors.fill: parent
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "Reset Database"; font.pixelSize: 16; font.bold: true; color: "#cdd6f4" }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Clears everything — accounts, categories, hashes, transactions. Starts from a "
                                + "completely blank database."
                            font.pixelSize: 13
                            color: "#6c7086"
                        }
                    }
                    PillButton {
                        text: "Reset Database"
                        variant: "danger"
                        Layout.preferredHeight: 32
                        onClicked: confirmDialog.openFor("reset")
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#313244" }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "Reset to Mock Data"; font.pixelSize: 16; font.bold: true; color: "#cdd6f4" }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Clears everything, then reseeds the default categories, account categories, "
                                + "account types, demo accounts, and a few sample transactions."
                            font.pixelSize: 13
                            color: "#6c7086"
                        }
                    }
                    PillButton {
                        text: "Reset to Mock Data"
                        variant: "primary"
                        Layout.preferredHeight: 32
                        onClicked: confirmDialog.openFor("mock")
                    }
                }
            }
        }

        // ── Per-account cleanup — remove specific data instead of wiping
        //    everything (see ARCHITECTURE.MD § Database Management). "Clear
        //    transactions" leaves the account/hashes/checkpoints in place;
        //    "Delete account" reuses AccountController.removeAccount()'s
        //    existing cascade (hashes + transactions + balance checkpoints —
        //    see Database::removeAccount()), same action as the "×" button
        //    on Manage Accounts, offered here too for convenience.
        ChartCard {
            Layout.fillWidth: true
            implicitHeight: cleanupCol.implicitHeight + 50
            title: "Per-account cleanup"

            ColumnLayout {
                id: cleanupCol
                anchors.fill: parent
                spacing: 14

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Remove data for a single account instead of resetting the whole database. Both "
                        + "actions below are irreversible."
                    font.pixelSize: 13
                    color: "#6c7086"
                }

                EmptyState {
                    visible: AccountController.accounts.length === 0
                    message: "No accounts yet."
                }

                RowLayout {
                    visible: AccountController.accounts.length > 0
                    Layout.fillWidth: true
                    spacing: 16

                    ComboBox {
                        id: cleanupAccountBox
                        Layout.preferredWidth: 260
                        model: AccountController.accounts
                        textRole: "name"
                    }

                    Item { Layout.fillWidth: true }

                    PillButton {
                        text: "Clear transactions"
                        variant: "neutral"
                        Layout.preferredHeight: 32
                        enabled: cleanupAccountBox.currentIndex >= 0
                        onClicked: clearTransactionsDialog.openFor(cleanupAccountBox.currentIndex)
                    }
                    PillButton {
                        text: "Delete account"
                        variant: "danger"
                        Layout.preferredHeight: 32
                        enabled: cleanupAccountBox.currentIndex >= 0
                        onClicked: deleteAccountDialog.openFor(cleanupAccountBox.currentIndex)
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }

    // ── Clear transactions confirmation ───────────────────────────────────────
    ConfirmDialog {
        id: clearTransactionsDialog
        dialogTitle: "Clear transactions?"
        confirmText: "Clear transactions"
        width: 380

        property int accountIndex: -1

        function openFor(idx) {
            accountIndex = idx
            var acc = AccountController.accounts[idx]
            message = "This permanently deletes every saved transaction for “" + acc.name
                + "”. The account itself, its hashes, and its balance checkpoints are kept. "
                + "This cannot be undone."
            open()
        }

        onConfirmed: {
            var acc = AccountController.accounts[clearTransactionsDialog.accountIndex]
            if (acc) {
                controller.clearTransactionsForAccount(acc.id)
            }
        }
    }

    // ── Delete account confirmation ───────────────────────────────────────────
    ConfirmDialog {
        id: deleteAccountDialog
        dialogTitle: "Delete account?"
        confirmText: "Delete"
        width: 380

        property int accountIndex: -1

        function openFor(idx) {
            accountIndex = idx
            var acc = AccountController.accounts[idx]
            message = "This permanently deletes “" + acc.name + "”, all of its stored hash links, "
                + "balance checkpoints, and saved transactions. This cannot be undone."
            open()
        }

        onConfirmed: AccountController.removeAccount(deleteAccountDialog.accountIndex)
    }

    // ── Confirmation dialog (shared by both actions) ──────────────────────────
    ConfirmDialog {
        id: confirmDialog
        width: 380

        property string mode: "reset" // "reset" | "mock"

        function openFor(m) {
            mode = m
            dialogTitle = m === "reset" ? "Reset the database?" : "Reset to mock data?"
            message = m === "reset"
                ? "This permanently deletes every account, category, hash, and transaction. This cannot be undone."
                : "This permanently deletes everything currently stored, then reloads the default demo data. This cannot be undone."
            confirmText = m === "reset" ? "Reset" : "Reset to mock"
            open()
        }

        onConfirmed: {
            if (mode === "reset")
                controller.resetDatabase()
            else
                controller.resetToMock()
        }
    }
}
