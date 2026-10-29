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
                title: "Manage Accounts"
                onBackClicked: page.closeRequested()

                StatusBadge {
                    visible: AccountController.statusText.length > 0
                    text: AccountController.statusText
                    variant: AccountController.statusLevel
                }
                PillButton {
                    text: "＋  Add account"
                    variant: "primary"
                    onClicked: accountDialog.openForNew()
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Manage the accounts themselves here. Account types and categories are managed "
                    + "from Account Management → \"Account types & categories\". Hashes are the "
                    + "fingerprints statement import uses to auto-match an account — an account with "
                    + "none yet shows as \"not linked\" during import. No IBAN, BIC, or account number "
                    + "is stored on an account."
                font.pixelSize: 14
                color: "#6c7086"
            }

            // ── Accounts ──────────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: accsCol.implicitHeight + 50
                title: "Accounts (" + AccountController.accounts.length + ")"

                ColumnLayout {
                    id: accsCol
                    anchors.fill: parent
                    spacing: 14

                    EmptyState {
                        visible: AccountController.accounts.length === 0
                        message: "No accounts yet — add one above."
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
                                    color: "#cdd6f4"
                                }
                                Text {
                                    text: accRow.modelData.type
                                    font.pixelSize: 13
                                    color: "#6c7086"
                                }
                                Rectangle {
                                    visible: accRow.modelData.category.length > 0
                                    Layout.preferredWidth: catBadgeText.implicitWidth + 16
                                    Layout.preferredHeight: 20
                                    radius: 10
                                    color: Qt.rgba(1, 1, 1, 0.08)
                                    border.color: AccountController.accountCategoryColor(accRow.modelData.category) || "#6c7086"
                                    Text {
                                        id: catBadgeText
                                        anchors.centerIn: parent
                                        text: accRow.modelData.category
                                        font.pixelSize: 12
                                        color: AccountController.accountCategoryColor(accRow.modelData.category) || "#6c7086"
                                    }
                                }
                                StatusBadge {
                                    variant: accRow.modelData.linked ? "success" : "warning"
                                    pillHeight: 20
                                    text: accRow.modelData.linked
                                        ? ("✓ linked · " + accRow.modelData.hashCount + " hash" + (accRow.modelData.hashCount === 1 ? "" : "es"))
                                        : "⚠ not linked"
                                }
                                Item { Layout.fillWidth: true }

                                GhostButton {
                                    text: "Transactions"
                                    onClicked: page.viewTransactionsRequested(accRow.index)
                                }
                                GhostButton {
                                    text: "✎ Edit"
                                    textColor: "#89b4fa"
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
                                Text { text: accRow.modelData.currency; font.pixelSize: 12; color: "#6c7086" }
                                Text {
                                    visible: accRow.modelData.notes.length > 0
                                    Layout.fillWidth: true
                                    text: accRow.modelData.notes
                                    font.pixelSize: 12
                                    font.italic: true
                                    color: "#45475a"
                                    elide: Text.ElideRight
                                }
                            }

                            // Hashes row
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 4
                                spacing: 8

                                Text { text: "Hashes:"; font.pixelSize: 12; color: "#6c7086" }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Repeater {
                                        model: accRow.modelData.hashes
                                        delegate: Rectangle {
                                            id: hashChip
                                            required property var modelData
                                            required property int index
                                            width: hashChipRow.implicitWidth + 16
                                            height: 22
                                            radius: 11
                                            color: "#181825"
                                            border.color: "#313244"

                                            RowLayout {
                                                id: hashChipRow
                                                anchors.centerIn: parent
                                                spacing: 6
                                                Text {
                                                    text: hashChip.modelData.hash.substring(0, 10) + "… (" + hashChip.modelData.source + ")"
                                                    font.pixelSize: 11
                                                    font.family: "monospace"
                                                    color: "#a6adc8"
                                                }
                                                Text {
                                                    text: "×"
                                                    font.pixelSize: 13
                                                    font.bold: true
                                                    color: "#f38ba8"
                                                    TapHandler {
                                                        onTapped: AccountController.removeHash(accRow.index, hashChip.index)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                GhostButton {
                                    text: "＋ Add hash"
                                    textColor: "#a6e3a1"
                                    onClicked: addHashDialog.openFor(accRow.index)
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

                                Text { text: "Balance checkpoints:"; font.pixelSize: 12; color: "#6c7086" }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Repeater {
                                        model: (page.checkpointsTick, AccountController.balanceCheckpoints(accRow.index))
                                        delegate: Rectangle {
                                            id: checkpointChip
                                            required property var modelData
                                            required property int index
                                            width: checkpointChipRow.implicitWidth + 16
                                            height: 22
                                            radius: 11
                                            color: "#181825"
                                            border.color: "#313244"

                                            RowLayout {
                                                id: checkpointChipRow
                                                anchors.centerIn: parent
                                                spacing: 6
                                                Text {
                                                    text: checkpointChip.modelData.dateText + " · " + checkpointChip.modelData.balanceText
                                                        + (checkpointChip.modelData.note.length > 0 ? " (" + checkpointChip.modelData.note + ")" : "")
                                                    font.pixelSize: 11
                                                    color: "#a6adc8"
                                                }
                                                Text {
                                                    text: "×"
                                                    font.pixelSize: 13
                                                    font.bold: true
                                                    color: "#f38ba8"
                                                    TapHandler {
                                                        onTapped: {
                                                            AccountController.removeBalanceCheckpoint(accRow.index, checkpointChip.index)
                                                            page.checkpointsTick++
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                GhostButton {
                                    text: "＋ Add checkpoint"
                                    textColor: "#f9e2af"
                                    onClicked: addCheckpointDialog.openFor(accRow.index)
                                }
                            }

                            Rectangle { Layout.fillWidth: true; Layout.topMargin: 4; Layout.preferredHeight: 1; color: "#313244" }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }

    // ── Add / edit account dialog ─────────────────────────────────────────────
    FormDialog {
        id: accountDialog
        dialogTitle: accountDialog.editIndex < 0 ? "New account" : "Edit account"
        saveText: accountDialog.editIndex < 0 ? "Add" : "Save"
        width: 440

        property int editIndex: -1
        // The stored values of the edited account. A value that is not in its
        // list (a removed type or category, an unknown currency) gives the
        // ComboBox index -1: the box shows the value, and Save keeps it (#63).
        property string keptType: ""
        property string keptCategory: ""
        property string keptCurrency: ""

        function keptText(value) {
            return value.length > 0 ? value + " (kept)" : "(none)"
        }

        readonly property var currencyOptions: [
            { code: "EUR", label: "EUR €" },
            { code: "USD", label: "USD $" },
        ]

        function categoryNames() {
            return AccountController.accountCategories.map(function(c) { return c.name })
        }
        function typeNames() {
            return AccountController.accountTypes.map(function(t) { return t.name })
        }
        function currencyIndexForCode(code) {
            for (var i = 0; i < currencyOptions.length; ++i)
                if (currencyOptions[i].code === code)
                    return i
            return -1
        }

        function openForNew() {
            editIndex = -1
            keptType = ""
            keptCategory = ""
            keptCurrency = ""
            accNameRow.text = ""
            // -1 when there is no type, after "Reset Database" (#62).
            accTypeBox.currentIndex = AccountController.accountTypes.length > 0 ? 0 : -1
            accCategoryBox.currentIndex = -1
            accCurrencyBox.currentIndex = 0
            accNotesRow.text = ""
            open()
        }

        function openForEdit(idx) {
            var acc = AccountController.accounts[idx]
            editIndex = idx
            keptType = acc.type
            keptCategory = acc.category
            keptCurrency = acc.currency
            accNameRow.text = acc.name
            // -1 when the type of the account is not in the list (no type, or
            // a removed one): Save then keeps it, instead of the first type.
            accTypeBox.currentIndex = typeNames().indexOf(acc.type)
            var ci = categoryNames().indexOf(acc.category)
            accCategoryBox.currentIndex = ci
            accCurrencyBox.currentIndex = currencyIndexForCode(acc.currency)
            accNotesRow.text = acc.notes
            open()
        }

        FormRow { id: accNameRow; label: "Name"; labelWidth: 70; placeholderText: "e.g. Livret Jeune" }
        RowLayout {
            spacing: 10
            Text { Layout.preferredWidth: 70; text: "Type"; font.pixelSize: 15; color: "#6c7086" }
            ComboBox {
                id: accTypeBox
                Layout.fillWidth: true
                model: AccountController.accountTypes
                textRole: "name"
                displayText: currentIndex >= 0 ? currentText : accountDialog.keptText(accountDialog.keptType)
            }
        }
        // After "Reset Database" the list of types is empty (#62).
        Text {
            visible: AccountController.accountTypes.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "There is no account type. The account is saved without a type. To add types, go to "
                + "Account Management → \"Account types & categories\"."
            font.pixelSize: 13
            color: "#f9e2af"
        }
        RowLayout {
            spacing: 10
            Text { Layout.preferredWidth: 70; text: "Category"; font.pixelSize: 15; color: "#6c7086" }
            ComboBox {
                id: accCategoryBox
                Layout.fillWidth: true
                model: AccountController.accountCategories
                textRole: "name"
                displayText: currentIndex >= 0 ? currentText : accountDialog.keptText(accountDialog.keptCategory)
            }
        }
        RowLayout {
            spacing: 10
            Text { Layout.preferredWidth: 70; text: "Currency"; font.pixelSize: 15; color: "#6c7086" }
            ComboBox {
                id: accCurrencyBox
                Layout.preferredWidth: 140
                model: accountDialog.currencyOptions
                textRole: "label"
                displayText: currentIndex >= 0 ? currentText : accountDialog.keptText(accountDialog.keptCurrency)
            }
        }
        FormRow { id: accNotesRow; label: "Notes"; labelWidth: 70; placeholderText: "optional" }

        onSaveClicked: {
            var trimmed = accNameRow.text.trim()
            if (trimmed.length === 0)
                return
            // An index outside the list is not read: before #62, openForNew()
            // set index 0 on the empty list of types, and Save read types[0]
            // (TypeError). With no selection, a new account gets no type or
            // category, and an edited account keeps its own.
            var categories = AccountController.accountCategories
            var types = AccountController.accountTypes
            var ci = accCategoryBox.currentIndex
            var ti = accTypeBox.currentIndex
            var current = editIndex >= 0 ? AccountController.accounts[editIndex] : null
            var category = ci >= 0 && ci < categories.length ? categories[ci].name
                                                             : (current ? current.category : "")
            var type = ti >= 0 && ti < types.length ? types[ti].name : (current ? current.type : "")
            var cu = accCurrencyBox.currentIndex
            var currency = cu >= 0 && cu < accountDialog.currencyOptions.length
                ? accountDialog.currencyOptions[cu].code : (current ? current.currency : "EUR")
            if (editIndex < 0) {
                AccountController.addAccount(trimmed, type, category, currency, accNotesRow.text.trim())
            } else {
                AccountController.updateAccount(editIndex, trimmed, type, category, currency, accNotesRow.text.trim())
            }
            close()
        }
    }

    // ── Delete account confirmation ───────────────────────────────────────────
    ConfirmDialog {
        id: deleteAccountDialog
        dialogTitle: "Delete account?"
        confirmText: "Delete"
        width: 360

        property int accountIndex: -1

        function openFor(idx, name) {
            accountIndex = idx
            message = "This will remove “" + name + "”, all of its stored hash links, "
                + "and all of its saved transactions. This cannot be undone."
            open()
        }

        onConfirmed: AccountController.removeAccount(deleteAccountDialog.accountIndex)
    }

    // ── Add hash dialog ────────────────────────────────────────────────────────
    FormDialog {
        id: addHashDialog
        dialogTitle: "Add a hash"
        saveText: "Add"
        width: 420

        property int accountIndex: -1

        function openFor(idx) { accountIndex = idx; hashField.text = ""; open() }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Paste a fingerprint (64 hexadecimal characters) that this account should also match on "
                + "import, for example the one of another statement layout of the same account. A fingerprint "
                + "belongs to one account only. Most accounts don't need this — link an account straight from "
                + "the import review screen instead (\"Link this account\")."
            font.pixelSize: 13
            color: "#6c7086"
        }
        // Not a FormRow — this dialog has no label column, just a full-width
        // field, so it uses the same TextField recipe directly.
        TextField {
            id: hashField
            Layout.fillWidth: true
            placeholderText: "64 hexadecimal characters"
            placeholderTextColor: "#45475a"
            font.family: "monospace"
            color: "#cdd6f4"
            font.pixelSize: 14
            background: Rectangle { color: "#11111b"; radius: 4; border.color: hashField.activeFocus ? "#89b4fa" : "#313244" }
        }

        onSaveClicked: {
            var trimmed = hashField.text.trim()
            if (trimmed.length === 0)
                return
            AccountController.addHash(addHashDialog.accountIndex, trimmed)
            close()
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
        dialogTitle: "Add a balance checkpoint"
        saveText: "Add"
        width: 420

        property int accountIndex: -1

        function openFor(idx) {
            accountIndex = idx
            checkpointDateRow.text = ""
            checkpointBalanceRow.text = ""
            checkpointNoteRow.text = ""
            open()
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Record a known balance for this account on a given date — used to validate a "
                + "statement import's declared opening balance when full transaction history since "
                + "account opening hasn't been imported (see the import review screen's "
                + "reconciliation check)."
            font.pixelSize: 13
            color: "#6c7086"
        }
        FormRow { id: checkpointDateRow; label: "Date"; labelWidth: 70; placeholderText: "yyyy-MM-dd" }
        FormRow { id: checkpointBalanceRow; label: "Balance"; labelWidth: 70; placeholderText: "e.g. 1250.00" }
        FormRow { id: checkpointNoteRow; label: "Note"; labelWidth: 70; placeholderText: "optional" }

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
