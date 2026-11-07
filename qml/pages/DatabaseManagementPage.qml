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
// The Test app (#130) also has "Copy my prod database", in its own card at the
// top: it replaces the Test data with a copy of the prod database. The card is
// there only when prodDatabasePath is set (main.cpp → Main.qml, Test app only).
//
// Built from the shared qml/components/ (PageHeader, StatusBadge,
// PillButton, ConfirmDialog) — see ARCHITECTURE.MD § Shared Components.

Item {
    id: page

    // The database of the prod app; empty in the prod app (see above).
    property string prodDatabasePath: ""

    DatabaseManagementController {
        id: controller
        prodDatabasePath: page.prodDatabasePath
        onDatabaseReset: {
            CategoryController.reload()
            AccountController.reload()
        }
    }

    // The prod app can change its database while the Test app runs.
    onVisibleChanged: {
        if (visible && page.prodDatabasePath.length > 0) {
            controller.refreshProdDatabase()
        }
    }

    // Scrolls: in the Test app, the copy card and its report make the page
    // higher than the window (#130).
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
                title: qsTr("Database Management")

                StatusBadge {
                    visible: controller.statusText.length > 0
                    text: controller.statusText
                    variant: controller.statusLevel
                }
            }

            // ── Copy of the prod data — the Test app only (#130) ─────────────────
            ChartCard {
                visible: page.prodDatabasePath.length > 0
                Layout.fillWidth: true
                implicitHeight: copyCol.implicitHeight + 50
                title: qsTr("Test app")

                ColumnLayout {
                    id: copyCol
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: qsTr("Copy my prod database"); font.pixelSize: 16; font.bold: true; color: Theme.text }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Replaces the data of this Test app with a copy of the data of Finance Dashboard. "
                                    + "The prod database is only read: it does not change. If the copy has an older "
                                    + "schema, the Test app upgrades it, as a new version of the prod app would. The "
                                    + "Test data from before the copy is kept in a file.")
                                font.pixelSize: 13
                                color: Theme.overlay0
                            }
                            Text {
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                wrapMode: Text.Wrap
                                text: controller.prodDatabaseText
                                font.pixelSize: 13
                                color: controller.prodDatabaseFound ? Theme.subtext0 : Theme.yellow
                            }
                        }
                        PillButton {
                            text: qsTr("Copy my prod database")
                            variant: "primary"
                            Layout.preferredHeight: 32
                            enabled: controller.prodDatabaseFound
                            onClicked: copyDialog.open()
                        }
                    }

                    // What the last copy did, or why it failed.
                    Text {
                        visible: controller.copyReport.length > 0
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: controller.copyReport
                        font.pixelSize: 13
                        color: controller.copyFailed ? Theme.red : Theme.subtext0
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Both actions wipe every table in the local SQLite database. Useful when the schema "
                    + "changes during development, or to start over. Neither action can be undone.")
                font.pixelSize: 15
                color: Theme.overlay0
            }

            ChartCard {
                Layout.fillWidth: true
                implicitHeight: actionsCol.implicitHeight + 50
                title: qsTr("Reset actions")

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
                            Text { text: qsTr("Reset Database"); font.pixelSize: 16; font.bold: true; color: Theme.text }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Clears everything — accounts, categories, hashes, transactions. Starts from a "
                                    + "completely blank database.")
                                font.pixelSize: 13
                                color: Theme.overlay0
                            }
                        }
                        PillButton {
                            text: qsTr("Reset Database")
                            variant: "danger"
                            Layout.preferredHeight: 32
                            onClicked: confirmDialog.openFor("reset")
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.surface0 }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: qsTr("Reset to Mock Data"); font.pixelSize: 16; font.bold: true; color: Theme.text }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Clears everything, then reseeds the default categories, account categories, "
                                    + "account types, demo accounts, and a few sample transactions.")
                                font.pixelSize: 13
                                color: Theme.overlay0
                            }
                        }
                        PillButton {
                            text: qsTr("Reset to Mock Data")
                            variant: "primary"
                            Layout.preferredHeight: 32
                            onClicked: confirmDialog.openFor("mock")
                        }
                    }
                }
            }

            // ── Category resets — the categories only; the accounts and the
            //    transactions stay (DatabaseManagementController::
            //    resetCategories(), Database::replaceCategories()).
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: categoryActionsCol.implicitHeight + 50
                title: qsTr("Categories")

                ColumnLayout {
                    id: categoryActionsCol
                    anchors.fill: parent
                    spacing: 16

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: qsTr("Reset categories to the defaults"); font.pixelSize: 16; font.bold: true; color: Theme.text }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Removes every category and subcategory, then adds back the default ones "
                                    + "(income, housing, subscriptions, investments…). Accounts and transactions stay.")
                                font.pixelSize: 13
                                color: Theme.overlay0
                            }
                        }
                        PillButton {
                            text: qsTr("Reset to defaults")
                            variant: "primary"
                            Layout.preferredHeight: 32
                            onClicked: confirmDialog.openFor("categoryDefaults")
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.surface0 }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: qsTr("Remove all categories"); font.pixelSize: 16; font.bold: true; color: Theme.text }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Removes every category and subcategory, to build your own list. Accounts "
                                    + "and transactions stay; the transactions become uncategorized.")
                                font.pixelSize: 13
                                color: Theme.overlay0
                            }
                        }
                        PillButton {
                            text: qsTr("Remove all")
                            variant: "danger"
                            Layout.preferredHeight: 32
                            onClicked: confirmDialog.openFor("categories")
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
                title: qsTr("Per-account cleanup")

                ColumnLayout {
                    id: cleanupCol
                    anchors.fill: parent
                    spacing: 14

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Remove data for a single account instead of resetting the whole database. Both "
                            + "actions below are irreversible.")
                        font.pixelSize: 13
                        color: Theme.overlay0
                    }

                    EmptyState {
                        visible: AccountController.accounts.length === 0
                        message: qsTr("No accounts yet.")
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
                            text: qsTr("Clear transactions")
                            variant: "neutral"
                            Layout.preferredHeight: 32
                            enabled: cleanupAccountBox.currentIndex >= 0
                                && cleanupAccountBox.currentIndex < AccountController.accounts.length
                            onClicked: clearTransactionsDialog.openFor(cleanupAccountBox.currentIndex)
                        }
                        PillButton {
                            text: qsTr("Delete account")
                            variant: "danger"
                            Layout.preferredHeight: 32
                            enabled: cleanupAccountBox.currentIndex >= 0
                                && cleanupAccountBox.currentIndex < AccountController.accounts.length
                            onClicked: deleteAccountDialog.openFor(cleanupAccountBox.currentIndex)
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }

    // ── Clear transactions confirmation ───────────────────────────────────────
    ConfirmDialog {
        id: clearTransactionsDialog
        dialogTitle: qsTr("Clear transactions?")
        confirmText: qsTr("Clear transactions")
        width: 380

        // The id, not the index: the list can change while the dialog is open (#67).
        property int accountId: -1

        function openFor(idx) {
            var acc = idx >= 0 && idx < AccountController.accounts.length ? AccountController.accounts[idx] : null
            if (!acc)
                return
            accountId = acc.id
            message = qsTr("This permanently deletes every saved transaction for “%1”. "
                + "The account itself, its hashes, and its balance checkpoints are kept. "
                + "This cannot be undone.").arg(acc.name)
            open()
        }

        onConfirmed: {
            if (clearTransactionsDialog.accountId >= 0)
                controller.clearTransactionsForAccount(clearTransactionsDialog.accountId)
        }
    }

    // ── Delete account confirmation ───────────────────────────────────────────
    ConfirmDialog {
        id: deleteAccountDialog
        dialogTitle: qsTr("Delete account?")
        confirmText: qsTr("Delete")
        width: 380

        // The id, not the index: removeAccount() takes an index, and the list can
        // change while the dialog is open. The index is read again on confirm, so
        // the dialog never deletes another account (#67).
        property int accountId: -1

        function openFor(idx) {
            var acc = idx >= 0 && idx < AccountController.accounts.length ? AccountController.accounts[idx] : null
            if (!acc)
                return
            accountId = acc.id
            message = qsTr("This permanently deletes “%1”, all of its stored hash links, "
                + "balance checkpoints, and saved transactions. This cannot be undone.").arg(acc.name)
            open()
        }

        onConfirmed: {
            var accounts = AccountController.accounts
            for (var i = 0; i < accounts.length; i++) {
                if (accounts[i].id === deleteAccountDialog.accountId) {
                    AccountController.removeAccount(i)
                    return
                }
            }
        }
    }

    // ── Copy my prod database confirmation (the Test app only, #130) ─────────
    ConfirmDialog {
        id: copyDialog
        width: 400
        dialogTitle: qsTr("Copy the prod database?")
        message: qsTr("The data of this Test app is replaced with a copy of the prod database. The prod "
            + "database does not change. The Test data from before is kept in a file next to the "
            + "Test database.")
        confirmText: qsTr("Copy")
        confirmVariant: "primary"
        onConfirmed: controller.copyProdDatabase()
    }

    // ── Confirmation dialog (shared by the four actions) ─────────────────────
    ConfirmDialog {
        id: confirmDialog
        width: 380

        property string mode: "reset" // "reset" | "mock" | "categories" | "categoryDefaults"

        function openFor(m) {
            mode = m
            if (m === "reset") {
                dialogTitle = qsTr("Reset the database?")
                message = qsTr("This permanently deletes every account, category, hash, and transaction. This cannot be undone.")
                confirmText = qsTr("Reset")
            } else if (m === "mock") {
                dialogTitle = qsTr("Reset to mock data?")
                message = qsTr("This permanently deletes everything currently stored, then reloads the default demo data. This cannot be undone.")
                confirmText = qsTr("Reset to mock")
            } else if (m === "categoryDefaults") {
                dialogTitle = qsTr("Reset the categories to the defaults?")
                message = qsTr("Every category and subcategory is replaced with the default ones. A transaction keeps its "
                    + "category when a default category has the same name; the others become uncategorized. "
                    + "The learned rules of a removed category are deleted. This cannot be undone.")
                confirmText = qsTr("Reset to defaults")
            } else {
                dialogTitle = qsTr("Remove all categories?")
                message = qsTr("Every category and subcategory is deleted. All transactions become uncategorized, and the "
                    + "learned category rules are deleted. Accounts and transactions stay. This cannot be undone.")
                confirmText = qsTr("Remove all")
            }
            open()
        }

        onConfirmed: {
            if (mode === "reset")
                controller.resetDatabase()
            else if (mode === "mock")
                controller.resetToMock()
            else if (mode === "categoryDefaults")
                controller.resetCategoriesToDefaults()
            else
                controller.resetCategories()
        }
    }
}
