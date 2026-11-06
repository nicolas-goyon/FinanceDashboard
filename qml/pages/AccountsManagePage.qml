pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Accounts Management — Account Management → "Manage Accounts" ─────────────
// Add/edit/remove accounts and manage each account's stored hash
// fingerprints (used to auto-match a statement import to an account — see
// ARCHITECTURE.MD § Statement Import). Account *type*/*category* lists
// themselves are managed on the separate "Account types & categories" page
// (AccountTypesCategoriesPage.qml, also under Account Management) — this
// page only picks from those lists per account, it doesn't CRUD them.
// Backed by AccountController (QML_SINGLETON,
// src/controllers/dataentry/AccountController.h) — the same shared instance
// read by ImportSnapshotPage.qml (target-account picker), so edits made
// here show up immediately there too.
//
// No iban/bic/account-number fields anywhere on this page — the app does
// not keep them (product decision, see ARCHITECTURE.MD § Account
// Management).
//
// "Transactions" opens a full routed page (AccountTransactionsPage.qml), not
// a modal — see viewTransactionsRequested() below and Main.qml's overlay
// wiring.
//
// Built from the shared qml/components/ (PageHeader, GhostButton,
// FormDialog, FormRow, ConfirmDialog, EmptyState, StatusBadge) — see
// ARCHITECTURE.MD § Shared Components.

Item {
    id: page

    signal closeRequested()
    // Emitted when "Transactions" is tapped on an account row; Main.qml
    // listens for this and swaps to the AccountTransactionsPage.qml overlay
    // for that account (see § Sidebar Navigation → Overlay pages).
    signal viewTransactionsRequested(int accountIndex)

    // Bumped after every checkpoint add/remove so the "Balance checkpoints"
    // row's model binding below re-evaluates — AccountController.
    // balanceCheckpoints() is a Q_INVOKABLE (reads straight from Database
    // every call, see its doc comment), not a NOTIFYable property, so QML
    // won't know to re-run that binding on its own (same reasoning as
    // AccountTransactionsPage.qml's manual "rows" refresh).
    property int checkpointsTick: 0

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
                title: qsTr("Manage Accounts")
                onBackClicked: page.closeRequested()

                StatusBadge {
                    visible: AccountController.statusText.length > 0
                    text: AccountController.statusText
                    variant: AccountController.statusLevel
                }
                PillButton {
                    text: qsTr("＋  Add account")
                    variant: "primary"
                    onClicked: accountDialog.openForNew()
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Manage the accounts themselves here. Account types and categories are managed "
                    + "from Account Management → \"Account types & categories\". The import link lets "
                    + "the statement import suggest the account; an account without one shows as "
                    + "\"not linked\" until its first import. No IBAN, BIC, or account number is stored "
                    + "on an account.")
                font.pixelSize: 14
                color: Theme.overlay0
            }

            // ── Accounts ──────────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: accsCol.implicitHeight + 50
                title: qsTr("Accounts (%1)").arg(AccountController.accounts.length)

                ColumnLayout {
                    id: accsCol
                    anchors.fill: parent
                    spacing: 14

                    EmptyState {
                        visible: AccountController.accounts.length === 0
                        message: qsTr("No accounts yet — add one above.")
                    }

                    Repeater {
                        model: AccountController.accounts
                        delegate: ColumnLayout {
                            id: accRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 8

                            // Header row: name, type, category badge, actions
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: accRow.modelData.name
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: Theme.text
                                }
                                Text {
                                    text: accRow.modelData.type
                                    font.pixelSize: 13
                                    color: Theme.overlay0
                                }
                                Rectangle {
                                    visible: accRow.modelData.category.length > 0
                                    Layout.preferredWidth: catBadgeText.implicitWidth + 16
                                    Layout.preferredHeight: 20
                                    radius: 10
                                    color: Qt.rgba(1, 1, 1, 0.08)
                                    border.color: AccountController.accountCategoryColor(accRow.modelData.category) || Theme.overlay0
                                    Text {
                                        id: catBadgeText
                                        anchors.centerIn: parent
                                        text: accRow.modelData.category
                                        font.pixelSize: 12
                                        color: AccountController.accountCategoryColor(accRow.modelData.category) || Theme.overlay0
                                    }
                                }
                                StatusBadge {
                                    variant: accRow.modelData.linked ? "success" : "warning"
                                    pillHeight: 20
                                    text: accRow.modelData.linked ? qsTr("✓ linked") : qsTr("⚠ not linked")
                                }
                                // #206: an account outside the net worth (a joint
                                // account, the account of a child).
                                StatusBadge {
                                    visible: !accRow.modelData.inNetWorth
                                    variant: "warning"
                                    pillHeight: 20
                                    text: qsTr("outside net worth")
                                }
                                Item { Layout.fillWidth: true }

                                GhostButton {
                                    text: qsTr("Transactions")
                                    onClicked: page.viewTransactionsRequested(accRow.index)
                                }
                                GhostButton {
                                    text: qsTr("✎ Edit")
                                    textColor: Theme.blue
                                    onClicked: accountDialog.openForEdit(accRow.index)
                                }
                                GhostButton {
                                    text: "×"
                                    variant: "danger"
                                    fontSize: 17
                                    bold: true
                                    horizontalPadding: 6
                                    onClicked: deleteAccountDialog.openFor(accRow.index, accRow.modelData.name)
                                }
                            }

                            // Details row: currency/notes
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 4
                                spacing: 20
                                Text { text: accRow.modelData.currency; font.pixelSize: 12; color: Theme.overlay0 }
                                Text {
                                    visible: accRow.modelData.notes.length > 0
                                    Layout.fillWidth: true
                                    text: accRow.modelData.notes
                                    font.pixelSize: 12
                                    font.italic: true
                                    color: Theme.surface1
                                    elide: Text.ElideRight
                                }
                            }

                            // Import link row (#203): the tag of the statements
                            // of this account. Its value tells nothing to the
                            // user, so the row shows only whether there is one.
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 4
                                spacing: 8

                                Text { text: qsTr("Import:"); font.pixelSize: 12; color: Theme.overlay0 }
                                Text {
                                    Layout.fillWidth: true
                                    text: accRow.modelData.linked
                                          ? qsTr("linked — the import suggests this account for its statements")
                                          : qsTr("not linked — the next import links it")
                                    font.pixelSize: 12
                                    color: accRow.modelData.linked ? Theme.subtext0 : Theme.surface1
                                    elide: Text.ElideRight
                                }
                                // Asks first: without the link, the import no longer
                                // suggests this account by itself (#78).
                                GhostButton {
                                    visible: accRow.modelData.linked
                                    text: qsTr("Forget link")
                                    variant: "danger"
                                    onClicked: forgetLinkDialog.openFor(accRow.modelData.id, accRow.modelData.name)
                                }
                            }

                            // Balance checkpoints row — the "magic number" anchor points
                            // (see ARCHITECTURE.MD § Balance Checkpoints): "the balance of
                            // this account was X on this date", used by statement-import
                            // reconciliation when full transaction history since account
                            // opening can't be imported.
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 4
                                spacing: 8

                                // #208: on an investment account, a checkpoint is a valuation
                                // (the value of the holdings at a date).
                                Text {
                                    text: accRow.modelData.isInvestment ? qsTr("Valuations:") : qsTr("Balance checkpoints:")
                                    font.pixelSize: 12
                                    color: Theme.overlay0
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Repeater {
                                        // Reads checkpointsTick so that the list is read again after a
                                        // change: balanceCheckpoints() has no notify signal.
                                        model: page.checkpointsTick >= 0
                                            ? AccountController.balanceCheckpoints(accRow.index) : []
                                        delegate: Rectangle {
                                            id: checkpointChip
                                            required property var modelData
                                            required property int index
                                            width: checkpointChipRow.implicitWidth + 16
                                            height: 22
                                            radius: 11
                                            color: Theme.mantle
                                            border.color: Theme.surface0

                                            RowLayout {
                                                id: checkpointChipRow
                                                anchors.centerIn: parent
                                                spacing: 6
                                                Text {
                                                    text: checkpointChip.modelData.dateText + " · " + checkpointChip.modelData.balanceText
                                                        + (checkpointChip.modelData.note.length > 0 ? " (" + checkpointChip.modelData.note + ")" : "")
                                                    font.pixelSize: 11
                                                    color: Theme.subtext0
                                                }
                                                GhostButton {
                                                    text: "×"
                                                    variant: "danger"
                                                    bold: true
                                                    horizontalPadding: 4
                                                    Layout.preferredHeight: 18
                                                    onClicked: {
                                                        AccountController.removeBalanceCheckpoint(accRow.index, checkpointChip.index)
                                                        page.checkpointsTick++
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                GhostButton {
                                    text: accRow.modelData.isInvestment ? qsTr("＋ Add valuation") : qsTr("＋ Add checkpoint")
                                    textColor: Theme.yellow
                                    onClicked: addCheckpointDialog.openFor(accRow.index)
                                }
                            }

                            Rectangle { Layout.fillWidth: true; Layout.topMargin: 4; Layout.preferredHeight: 1; color: Theme.surface0 }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }

    // ── Add / edit account dialog (qml/components/AccountFormDialog.qml) ──────
    AccountFormDialog { id: accountDialog }

    // ── Delete account confirmation ───────────────────────────────────────────
    ConfirmDialog {
        id: deleteAccountDialog
        dialogTitle: qsTr("Delete account?")
        confirmText: qsTr("Delete")
        width: 360

        property int accountIndex: -1

        function openFor(idx, name) {
            accountIndex = idx
            message = qsTr("This will remove “%1”, all of its stored hash links, "
                + "and all of its saved transactions. This cannot be undone.").arg(name)
            open()
        }

        onConfirmed: AccountController.removeAccount(deleteAccountDialog.accountIndex)
    }

    // ── Forget import link confirmation (#78, #203) ───────────────────────────
    // Keeps the account id, not its position: the index is read again on
    // confirm.
    ConfirmDialog {
        id: forgetLinkDialog
        dialogTitle: qsTr("Forget the import link?")
        confirmText: qsTr("Forget")
        width: 380

        property int accountId: -1

        function openFor(accountId, accountName) {
            forgetLinkDialog.accountId = accountId
            message = qsTr("The import then no longer suggests “%1” by itself for the "
                + "statements of this bank account. The next import that you save to it links it again.").arg(accountName)
            open()
        }

        onConfirmed: {
            var accounts = AccountController.accounts
            for (var i = 0; i < accounts.length; i++) {
                if (accounts[i].id === forgetLinkDialog.accountId) {
                    AccountController.forgetImportLink(i)
                    return
                }
            }
        }
    }

    // ── Add balance checkpoint dialog ─────────────────────────────────────────
    // The "magic number" anchor point — see ARCHITECTURE.MD § Balance
    // Checkpoints. No date-picker widget in this app yet (see Qt Graphs
    // examples/doc.qt.io — nothing to copy a pattern from), so the date is
    // plain "yyyy-MM-dd" text, parsed/validated in
    // AccountController::addBalanceCheckpoint().
    FormDialog {
        id: addCheckpointDialog
        dialogTitle: addCheckpointDialog.investment ? qsTr("Add a valuation") : qsTr("Add a balance checkpoint")
        saveText: qsTr("Add")
        width: 420

        property int accountIndex: -1
        property bool investment: false

        function openFor(idx) {
            accountIndex = idx
            investment = idx >= 0 && idx < AccountController.accounts.length && AccountController.accounts[idx].isInvestment
            checkpointDateRow.text = ""
            checkpointBalanceRow.text = ""
            checkpointNoteRow.text = ""
            open()
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: addCheckpointDialog.investment
                ? qsTr("Enter the value of the account on a date, from the statement of your bank. Between two "
                  + "valuations, the value is the last valuation plus the deposits and withdrawals after it. "
                  + "The change that is not a deposit is the gain or loss of the period.")
                : qsTr("Record a known balance for this account on a given date — used to validate a "
                  + "statement import's declared opening balance when full transaction history since "
                  + "account opening hasn't been imported (see the import review screen's "
                  + "reconciliation check).")
            font.pixelSize: 13
            color: Theme.overlay0
        }
        FormRow { id: checkpointDateRow; label: qsTr("Date"); labelWidth: 70; placeholderText: "yyyy-MM-dd" }
        FormRow {
            id: checkpointBalanceRow
            label: addCheckpointDialog.investment ? qsTr("Value") : qsTr("Balance")
            labelWidth: 70
            placeholderText: qsTr("e.g. 1250.00")
        }
        FormRow { id: checkpointNoteRow; label: qsTr("Note"); labelWidth: 70; placeholderText: qsTr("optional") }

        onSaveClicked: {
            var dateText = checkpointDateRow.text.trim()
            var balance = parseFloat(checkpointBalanceRow.text.trim().replace(",", "."))
            if (dateText.length === 0 || isNaN(balance))
                return
            var ok = AccountController.addBalanceCheckpoint(addCheckpointDialog.accountIndex, dateText, balance, checkpointNoteRow.text.trim())
            if (ok) {
                page.checkpointsTick++
                close()
            }
        }
    }
}
