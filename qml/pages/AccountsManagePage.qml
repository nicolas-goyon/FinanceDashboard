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
// No iban/bic/account-number fields anywhere on this page — deliberately
// not stored on accounts for security reasons (see ARCHITECTURE.MD §
// Account Management).
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
                    variant: "success"
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
                                    width: catBadgeText.implicitWidth + 16
                                    height: 20
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

                            Rectangle { Layout.fillWidth: true; Layout.topMargin: 4; height: 1; color: "#313244" }
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }

    // ── Add / edit account dialog ─────────────────────────────────────────────
    FormDialog {
        id: accountDialog
        dialogTitle: accountDialog.editIndex < 0 ? "New account" : "Edit account"
        saveText: accountDialog.editIndex < 0 ? "Add" : "Save"
        width: 440

        property int editIndex: -1

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
            return 0
        }

        function openForNew() {
            editIndex = -1
            accNameRow.text = ""
            accTypeBox.currentIndex = 0
            accCategoryBox.currentIndex = -1
            accCurrencyBox.currentIndex = 0
            accNotesRow.text = ""
            open()
        }

        function openForEdit(idx) {
            var acc = AccountController.accounts[idx]
            editIndex = idx
            accNameRow.text = acc.name
            var ti = typeNames().indexOf(acc.type)
            accTypeBox.currentIndex = ti >= 0 ? ti : 0
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
            }
        }
        RowLayout {
            spacing: 10
            Text { Layout.preferredWidth: 70; text: "Category"; font.pixelSize: 15; color: "#6c7086" }
            ComboBox {
                id: accCategoryBox
                Layout.fillWidth: true
                model: AccountController.accountCategories
                textRole: "name"
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
            }
        }
        FormRow { id: accNotesRow; label: "Notes"; labelWidth: 70; placeholderText: "optional" }

        onSaveClicked: {
            var trimmed = accNameRow.text.trim()
            if (trimmed.length === 0)
                return
            var category = accCategoryBox.currentIndex >= 0
                ? AccountController.accountCategories[accCategoryBox.currentIndex].name : ""
            var type = accTypeBox.currentIndex >= 0
                ? AccountController.accountTypes[accTypeBox.currentIndex].name : ""
            var currency = accountDialog.currencyOptions[accCurrencyBox.currentIndex].code
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
            text: "Paste a fingerprint this account should also match on import (e.g. one printed by a "
                + "different statement layout for the same account). Most accounts don't need this — link "
                + "an account straight from the import review screen instead (\"Link this account\")."
            font.pixelSize: 13
            color: "#6c7086"
        }
        // Not a FormRow — this dialog has no label column, just a full-width
        // field, so it uses the same TextField recipe directly.
        TextField {
            id: hashField
            Layout.fillWidth: true
            placeholderText: "hash value"
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
}
