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
// read by TransactionsManagementPage.qml (snapshot form) and
// ImportSnapshotPage.qml (target-account picker), so edits made here show
// up immediately on both.
//
// No iban/bic/account-number fields anywhere on this page — deliberately
// not stored on accounts for security reasons (see ARCHITECTURE.MD §
// Account Management).
//
// "Transactions" opens a full routed page (AccountTransactionsPage.qml), not
// a modal — see viewTransactionsRequested() below and Main.qml's overlay
// wiring.

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
                    text: "Manage Accounts"
                    font.pixelSize: 23
                    font.bold: true
                    color: "#cdd6f4"
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    visible: AccountController.statusText.length > 0
                    width: statusLabel.implicitWidth + 20
                    height: 26
                    radius: 13
                    color: "#1a3328"
                    border.color: "#2d5a45"
                    Text {
                        id: statusLabel
                        anchors.centerIn: parent
                        text: AccountController.statusText
                        font.pixelSize: 14
                        color: "#a6e3a1"
                    }
                }
                Rectangle {
                    width: 150; height: 30; radius: 4
                    color: addAccHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text {
                        anchors.centerIn: parent
                        text: "＋  Add account"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#cdd6f4"
                    }
                    HoverHandler { id: addAccHover }
                    TapHandler { onTapped: accountDialog.openForNew() }
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

                    Text {
                        visible: AccountController.accounts.length === 0
                        text: "No accounts yet — add one above."
                        font.pixelSize: 15
                        color: "#6c7086"
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
                                Rectangle {
                                    width: linkBadgeText.implicitWidth + 16
                                    height: 20
                                    radius: 10
                                    color: accRow.modelData.linked ? "#1a3328" : "#3d3320"
                                    Text {
                                        id: linkBadgeText
                                        anchors.centerIn: parent
                                        text: accRow.modelData.linked
                                            ? ("✓ linked · " + accRow.modelData.hashCount + " hash" + (accRow.modelData.hashCount === 1 ? "" : "es"))
                                            : "⚠ not linked"
                                        font.pixelSize: 12
                                        color: accRow.modelData.linked ? "#a6e3a1" : "#f9e2af"
                                    }
                                }
                                Item { Layout.fillWidth: true }

                                Rectangle {
                                    width: 96; height: 22; radius: 4
                                    color: "transparent"
                                    border.color: "#313244"
                                    Text { anchors.centerIn: parent; text: "Transactions"; font.pixelSize: 12; color: "#89b4fa" }
                                    TapHandler { onTapped: page.viewTransactionsRequested(accRow.index) }
                                }
                                Rectangle {
                                    width: 60; height: 22; radius: 4
                                    color: "transparent"
                                    border.color: "#313244"
                                    Text { anchors.centerIn: parent; text: "✎ Edit"; font.pixelSize: 13; color: "#89b4fa" }
                                    TapHandler { onTapped: accountDialog.openForEdit(accRow.index) }
                                }
                                Rectangle {
                                    width: 22; height: 22; radius: 4
                                    color: removeAccHover.hovered ? "#3d2028" : "transparent"
                                    border.color: removeAccHover.hovered ? "#f38ba8" : "#313244"
                                    Text {
                                        anchors.centerIn: parent
                                        text: "×"
                                        font.pixelSize: 17
                                        font.bold: true
                                        color: removeAccHover.hovered ? "#f38ba8" : "#6c7086"
                                    }
                                    HoverHandler { id: removeAccHover }
                                    TapHandler { onTapped: deleteAccountDialog.openFor(accRow.index, accRow.modelData.name) }
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

                                Rectangle {
                                    width: 90; height: 22; radius: 4
                                    color: "transparent"
                                    border.color: "#313244"
                                    Text { anchors.centerIn: parent; text: "＋ Add hash"; font.pixelSize: 12; color: "#a6e3a1" }
                                    TapHandler { onTapped: addHashDialog.openFor(accRow.index) }
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
    Popup {
        id: accountDialog
        anchors.centerIn: parent
        width: 440
        padding: 20
        modal: true

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
            accNameField.text = ""
            accTypeBox.currentIndex = 0
            accCategoryBox.currentIndex = -1
            accCurrencyBox.currentIndex = 0
            accNotesField.text = ""
            open()
        }

        function openForEdit(idx) {
            var acc = AccountController.accounts[idx]
            editIndex = idx
            accNameField.text = acc.name
            var ti = typeNames().indexOf(acc.type)
            accTypeBox.currentIndex = ti >= 0 ? ti : 0
            var ci = categoryNames().indexOf(acc.category)
            accCategoryBox.currentIndex = ci
            accCurrencyBox.currentIndex = currencyIndexForCode(acc.currency)
            accNotesField.text = acc.notes
            open()
        }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: accountDialog.editIndex < 0 ? "New account" : "Edit account"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }

            RowLayout {
                spacing: 10
                Text { Layout.preferredWidth: 70; text: "Name"; font.pixelSize: 15; color: "#6c7086" }
                TextField {
                    id: accNameField
                    Layout.fillWidth: true
                    placeholderText: "e.g. Livret Jeune"
                    placeholderTextColor: "#45475a"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    background: Rectangle { color: "#11111b"; radius: 4; border.color: accNameField.activeFocus ? "#89b4fa" : "#313244" }
                }
            }
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
            RowLayout {
                spacing: 10
                Text { Layout.preferredWidth: 70; text: "Notes"; font.pixelSize: 15; color: "#6c7086" }
                TextField {
                    id: accNotesField
                    Layout.fillWidth: true
                    placeholderText: "optional"
                    placeholderTextColor: "#45475a"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    background: Rectangle { color: "#11111b"; radius: 4; border.color: accNotesField.activeFocus ? "#89b4fa" : "#313244" }
                }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: accCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: accCancelHover }
                    TapHandler { onTapped: accountDialog.close() }
                }
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: accSaveHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text {
                        anchors.centerIn: parent
                        text: accountDialog.editIndex < 0 ? "Add" : "Save"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#cdd6f4"
                    }
                    HoverHandler { id: accSaveHover }
                    TapHandler {
                        onTapped: {
                            var trimmed = accNameField.text.trim()
                            if (trimmed.length === 0)
                                return
                            var category = accCategoryBox.currentIndex >= 0
                                ? AccountController.accountCategories[accCategoryBox.currentIndex].name : ""
                            var type = accTypeBox.currentIndex >= 0
                                ? AccountController.accountTypes[accTypeBox.currentIndex].name : ""
                            var currency = accountDialog.currencyOptions[accCurrencyBox.currentIndex].code
                            if (accountDialog.editIndex < 0) {
                                AccountController.addAccount(trimmed, type, category, currency, accNotesField.text.trim())
                            } else {
                                AccountController.updateAccount(accountDialog.editIndex, trimmed, type, category, currency, accNotesField.text.trim())
                            }
                            accountDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ── Delete account confirmation ───────────────────────────────────────────
    Popup {
        id: deleteAccountDialog
        anchors.centerIn: parent
        width: 360
        padding: 20
        modal: true

        property int accountIndex: -1
        property string accountName: ""

        function openFor(idx, name) { accountIndex = idx; accountName = name; open() }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Delete account?"; font.pixelSize: 19; font.bold: true; color: "#cdd6f4" }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "This will remove “" + deleteAccountDialog.accountName + "”, all of its stored hash links, "
                    + "and all of its saved transactions. This cannot be undone."
                font.pixelSize: 14
                color: "#a6adc8"
            }
            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: delAccCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: delAccCancelHover }
                    TapHandler { onTapped: deleteAccountDialog.close() }
                }
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: delAccConfirmHover.hovered ? "#5a2d36" : "#3d2028"
                    border.color: "#f38ba8"
                    Text { anchors.centerIn: parent; text: "Delete"; font.pixelSize: 15; font.bold: true; color: "#f38ba8" }
                    HoverHandler { id: delAccConfirmHover }
                    TapHandler {
                        onTapped: {
                            AccountController.removeAccount(deleteAccountDialog.accountIndex)
                            deleteAccountDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ── Add hash dialog ────────────────────────────────────────────────────────
    Popup {
        id: addHashDialog
        anchors.centerIn: parent
        width: 420
        padding: 20
        modal: true

        property int accountIndex: -1

        function openFor(idx) { accountIndex = idx; hashField.text = ""; open() }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Add a hash"; font.pixelSize: 19; font.bold: true; color: "#cdd6f4" }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Paste a fingerprint this account should also match on import (e.g. one printed by a "
                    + "different statement layout for the same account). Most accounts don't need this — link "
                    + "an account straight from the import review screen instead (\"Link this account\")."
                font.pixelSize: 13
                color: "#6c7086"
            }
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
            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: hashCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: hashCancelHover }
                    TapHandler { onTapped: addHashDialog.close() }
                }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: hashAddHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text { anchors.centerIn: parent; text: "Add"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                    HoverHandler { id: hashAddHover }
                    TapHandler {
                        onTapped: {
                            var trimmed = hashField.text.trim()
                            if (trimmed.length === 0)
                                return
                            AccountController.addHash(addHashDialog.accountIndex, trimmed)
                            addHashDialog.close()
                        }
                    }
                }
            }
        }
    }
}
