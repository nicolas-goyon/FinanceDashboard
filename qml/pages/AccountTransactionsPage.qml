pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── View & Edit Transactions — Transactions category's own sidebar action,
//    also reached from Manage Accounts → "Transactions" ────────────────────
// Table of every persisted transaction for one account (see
// AccountController::transactionsForAccount() / db/tables/TransactionsTable.h)
// — the "button to access a table with all transactions registered at that
// account" from the product request. A full routed page (not a modal/Popup)
// per product decision.
//
// Two entry points as of 2026-07-18 (product feedback — see ARCHITECTURE.MD
// § Sidebar Navigation): the Transactions category's own "View & Edit
// Transactions" sidebar action (no preset account — accountIndex arrives as
// -1, this page picks its own default via `selectedAccountIndex` below and
// shows an account picker), and AccountsManagePage.qml's per-row
// "Transactions" button (emits viewTransactionsRequested(accountIndex),
// Main.qml sets accountIndex to that specific row). closeRequested() returns
// to whichever one you came from — Main.qml tracks that in
// accountTransactionsReturnViewId, not this page.
//
// CATEGORY/SUBCATEGORY show the category's/subcategory's own color (via
// CategoryController.categoryColor()/subcategoryColor()) as a small badge,
// same visual language as the account-category badge on
// AccountsManagePage.qml. Each row also has a "✎ Edit" action opening a
// modal to fix a mis-categorized or mis-entered persisted transaction (see
// ARCHITECTURE.MD § Account Management — editing was previously "read-only
// for this pass", now wired to AccountController::updateTransaction()).
//
// `rows` is a plain (not `readonly`) property, manually reloaded via
// reloadRows() — AccountController.transactionsForAccount() is a
// Q_INVOKABLE (reads straight from Database every call), not a NOTIFYable
// property, so a declarative binding wouldn't know to re-evaluate after an
// edit. Same reasoning as AccountsManagePage.qml's balance-checkpoints
// refresh tick.

Item {
    id: page

    signal closeRequested()

    // Set by Main.qml right before showing this overlay — index into
    // AccountController.accounts. -1 means "no specific account requested"
    // (the sidebar entry point) — see `selectedAccountIndex` below, which is
    // what the rest of this page actually reads.
    property int accountIndex: -1

    // The account actually shown — falls back to the first account when
    // accountIndex is -1, otherwise mirrors accountIndex. Kept as its own
    // property (rather than writing back into accountIndex, which Main.qml
    // binds declaratively) because an imperative write from the in-page
    // picker below would permanently break that binding — same hazard
    // documented on ColorComboBox.currentIndex (see ARCHITECTURE.MD §
    // Charting Gotchas / QML gotchas). Safe here because this page is torn
    // down and recreated fresh every time its Loader.active toggles on, same
    // as AccountHistoryPage.qml's own account picker (never reused across
    // opens like a Popup would be).
    property int selectedAccountIndex:
        page.accountIndex >= 0 ? page.accountIndex : (AccountController.accounts.length > 0 ? 0 : -1)

    readonly property var account:
        page.selectedAccountIndex >= 0 && page.selectedAccountIndex < AccountController.accounts.length
            ? AccountController.accounts[page.selectedAccountIndex] : null

    property var rows: []

    function reloadRows() {
        page.rows = page.selectedAccountIndex >= 0 ? AccountController.transactionsForAccount(page.selectedAccountIndex) : []
    }

    Component.onCompleted: page.reloadRows()
    onSelectedAccountIndexChanged: page.reloadRows()

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
                title: "Transactions" + (page.account !== null ? " — " + page.account.name : "")
                onBackClicked: page.closeRequested()

                // Account picker — only meaningful when there's more than one
                // account to switch between; harmless (just narrow) with 0/1.
                // Same imperative currentIndex pattern as
                // AccountHistoryPage.qml's picker — see selectedAccountIndex's
                // doc comment above for why plain ComboBox + onActivated is
                // safe here.
                ComboBox {
                    Layout.preferredWidth: 240
                    model: AccountController.accounts
                    textRole: "name"
                    currentIndex: page.selectedAccountIndex
                    onActivated: page.selectedAccountIndex = currentIndex
                }

                Text { text: page.rows.length + " row(s)"; font.pixelSize: 14; color: "#6c7086" }
            }

            Text {
                visible: AccountController.accounts.length === 0
                text: "No accounts yet — add one from Account Management → Manage Accounts."
                font.pixelSize: 15
                color: "#f38ba8"
            }

            Text {
                visible: AccountController.accounts.length > 0 && page.account === null
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
                            Text { Layout.preferredWidth: 260; text: "NAME";        font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            // Narrow column — rows only show a view-icon here (see
                            // descriptionPopup below); the fillWidth spacer after it takes up
                            // the space the old fillWidth DESCRIPTION text column used to, so
                            // DEBIT/CREDIT/… stay pinned to the right — same pattern as
                            // ImportSnapshotPage.qml's transaction table.
                            Text { Layout.preferredWidth: 44;  text: "DESC.";       font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            Item { Layout.fillWidth: true }
                            Text { Layout.preferredWidth: 90;  text: "DEBIT";       font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 90;  text: "CREDIT";      font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 100; text: "BALANCE";     font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 130; text: "CATEGORY";    font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            Text { Layout.preferredWidth: 130; text: "SUBCATEGORY"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            Text { Layout.preferredWidth: 60;  text: "";            font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                        }
                    }

                    EmptyState {
                        visible: page.rows.length === 0
                        Layout.topMargin: 10
                        message: "No transactions saved for this account yet."
                    }

                    Repeater {
                        model: page.rows
                        delegate: Rectangle {
                            id: txRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            color: index % 2 === 0 ? "transparent" : "#181825"

                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                spacing: 8
                                Text { Layout.preferredWidth: 90; text: txRow.modelData.operationDate; font.pixelSize: 14; color: "#6c7086" }
                                Text {
                                    Layout.preferredWidth: 260
                                    text: txRow.modelData.name.length > 0 ? txRow.modelData.name : "—"
                                    font.pixelSize: 15
                                    color: "#cdd6f4"
                                    elide: Text.ElideRight
                                }
                                GhostButton {
                                    Layout.preferredWidth: 44
                                    text: "🔍"
                                    onClicked: {
                                        descriptionPopup.descriptionText = txRow.modelData.description
                                        descriptionPopup.open()
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                Text { Layout.preferredWidth: 90;  text: txRow.modelData.debitText;  font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 90;  text: txRow.modelData.creditText; font.pixelSize: 15; color: "#a6e3a1"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 100; text: txRow.modelData.balanceText; font.pixelSize: 14; color: "#6c7086"; horizontalAlignment: Text.AlignRight }

                                // Category/subcategory badges — same "colored pill" language
                                // as the account-category badge on AccountsManagePage.qml,
                                // colors resolved via CategoryController (see that
                                // singleton's categoryColor()/subcategoryColor() convenience
                                // lookups — ARCHITECTURE.MD § Category Management).
                                Rectangle {
                                    Layout.preferredWidth: 130
                                    visible: txRow.modelData.category.length > 0
                                    Layout.preferredHeight: 20
                                    radius: 10
                                    color: Qt.rgba(1, 1, 1, 0.08)
                                    border.color: CategoryController.categoryColor(txRow.modelData.category) || "#6c7086"
                                    Text {
                                        anchors.centerIn: parent
                                        text: txRow.modelData.category
                                        font.pixelSize: 12
                                        color: CategoryController.categoryColor(txRow.modelData.category) || "#6c7086"
                                        elide: Text.ElideRight
                                        width: Math.min(implicitWidth, 118)
                                    }
                                }
                                Text { Layout.preferredWidth: 130; visible: txRow.modelData.category.length === 0; text: "—"; font.pixelSize: 14; color: "#6c7086" }

                                Rectangle {
                                    Layout.preferredWidth: 130
                                    visible: txRow.modelData.subcategory.length > 0
                                    Layout.preferredHeight: 20
                                    radius: 10
                                    color: Qt.rgba(1, 1, 1, 0.08)
                                    border.color: CategoryController.subcategoryColor(txRow.modelData.category, txRow.modelData.subcategory) || "#6c7086"
                                    Text {
                                        anchors.centerIn: parent
                                        text: txRow.modelData.subcategory
                                        font.pixelSize: 12
                                        color: CategoryController.subcategoryColor(txRow.modelData.category, txRow.modelData.subcategory) || "#6c7086"
                                        elide: Text.ElideRight
                                        width: Math.min(implicitWidth, 118)
                                    }
                                }
                                Text { Layout.preferredWidth: 130; visible: txRow.modelData.subcategory.length === 0; text: "—"; font.pixelSize: 14; color: "#6c7086" }

                                GhostButton {
                                    Layout.preferredWidth: 60
                                    text: "✎ Edit"
                                    textColor: "#89b4fa"
                                    onClicked: editDialog.openFor(txRow.modelData)
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }

    // ── Full-description popup ──────────────────────────────────────────────────
    // Opened by the 🔍 icon in the transaction table — same shared-instance
    // pattern as ImportSnapshotPage.qml's descriptionPopup. Text is selectable
    // (TextEdit.selectByMouse) so the raw bank description can be copy-pasted.
    Popup {
        id: descriptionPopup
        anchors.centerIn: parent
        width: 480
        height: 280
        padding: 16
        modal: true

        property string descriptionText: ""

        background: Rectangle {
            color: "#181825"
            border.color: "#45475a"
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text { text: "Full description"; font.pixelSize: 18; font.bold: true; color: "#cdd6f4" }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextEdit {
                    width: descriptionPopup.width - 32
                    text: descriptionPopup.descriptionText
                    readOnly: true
                    selectByMouse: true
                    persistentSelection: true
                    wrapMode: TextEdit.WordWrap
                    font.pixelSize: 15
                    color: "#cdd6f4"
                    selectionColor: "#89b4fa"
                    selectedTextColor: "#11111b"
                }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                PillButton { text: "Close"; variant: "neutral"; onClicked: descriptionPopup.close() }
            }
        }
    }

    // ── Edit transaction dialog ───────────────────────────────────────────────
    // Backs the "✎ Edit" action above — writes through
    // AccountController.updateTransaction() (by the row's real DB id, not
    // its position in `rows`). balance_after is deliberately not editable
    // here — see TransactionsTable::updateById()'s doc comment: editing
    // debit/credit does NOT recompute the running balance chain for this or
    // any later row, a known limitation.
    //
    // Plain ComboBoxes (not ColorComboBox) for category/subcategory,
    // deliberately — same reasoning as AccountsManagePage.qml's account
    // edit dialog: ColorComboBox.currentIndex is a *declarative* binding to
    // `currentName` that permanently breaks the first time the user picks
    // an item (see ColorComboBox.qml's own doc comment), which is fine only
    // when the ComboBox lives inside a Repeater delegate that gets
    // recreated on every model change. This dialog's Popup.contentItem is
    // NOT recreated between opens (one shared instance, reused for every
    // row's "✎ Edit" click), so it needs the imperative-currentIndex
    // pattern instead — set once in openFor(), read back in onSaveClicked.
    FormDialog {
        id: editDialog
        dialogTitle: "Edit transaction"
        saveText: "Save"
        width: 460

        property int transactionId: -1

        function categoryNames() {
            return CategoryController.categories.map(function(c) { return c.name })
        }
        function subcategoriesFor(categoryName) {
            var cat = CategoryController.categories.find(function(c) { return c.name === categoryName })
            return cat ? cat.subcategories : []
        }
        function subcategoryNames(categoryName) {
            return subcategoriesFor(categoryName).map(function(s) { return s.name })
        }

        function openFor(row) {
            transactionId = row.id
            editConfirmationDateRow.text = row.confirmationDateIso
            editOperationDateRow.text = row.operationDateIso
            editNameRow.text = row.name
            editDescriptionRow.text = row.description
            editDebitRow.text = row.debit > 0 ? String(row.debit) : ""
            editCreditRow.text = row.credit > 0 ? String(row.credit) : ""

            editCategoryBox.model = categoryNames()
            var ci = categoryNames().indexOf(row.category)
            editCategoryBox.currentIndex = ci

            editSubcategoryBox.model = subcategoryNames(row.category)
            var si = subcategoryNames(row.category).indexOf(row.subcategory)
            editSubcategoryBox.currentIndex = si

            open()
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Balance shown in the table is not recalculated after an edit — fix debit/credit "
                + "here for categorization/bookkeeping, not to correct the bank's own running balance."
            font.pixelSize: 12
            color: "#6c7086"
        }
        FormRow { id: editConfirmationDateRow; label: "Confirm. date"; labelWidth: 110; placeholderText: "yyyy-MM-dd" }
        FormRow { id: editOperationDateRow; label: "Operation date"; labelWidth: 110; placeholderText: "yyyy-MM-dd" }
        FormRow { id: editNameRow; label: "Name"; labelWidth: 110 }
        FormRow { id: editDescriptionRow; label: "Description"; labelWidth: 110 }
        FormRow { id: editDebitRow; label: "Debit"; labelWidth: 110; placeholderText: "0.00" }
        FormRow { id: editCreditRow; label: "Credit"; labelWidth: 110; placeholderText: "0.00" }
        RowLayout {
            spacing: 10
            Text { Layout.preferredWidth: 110; text: "Category"; font.pixelSize: 15; color: "#6c7086" }
            ComboBox {
                id: editCategoryBox
                Layout.fillWidth: true
                onActivated: {
                    editSubcategoryBox.model = editDialog.subcategoryNames(editCategoryBox.currentText)
                    editSubcategoryBox.currentIndex = -1
                }
            }
        }
        RowLayout {
            spacing: 10
            Text { Layout.preferredWidth: 110; text: "Subcategory"; font.pixelSize: 15; color: "#6c7086" }
            ComboBox {
                id: editSubcategoryBox
                Layout.fillWidth: true
                enabled: count > 0
            }
        }

        onSaveClicked: {
            var debit = parseFloat(editDebitRow.text.trim().replace(",", "."))
            var credit = parseFloat(editCreditRow.text.trim().replace(",", "."))
            if (isNaN(debit)) debit = 0
            if (isNaN(credit)) credit = 0
            var category = editCategoryBox.currentIndex >= 0 ? editCategoryBox.currentText : ""
            var subcategory = editSubcategoryBox.currentIndex >= 0 ? editSubcategoryBox.currentText : ""
            var ok = AccountController.updateTransaction(
                editDialog.transactionId,
                editConfirmationDateRow.text.trim(),
                editOperationDateRow.text.trim(),
                editNameRow.text.trim(),
                editDescriptionRow.text.trim(),
                debit, credit,
                category, subcategory)
            if (ok) {
                page.reloadRows()
                close()
            }
        }
    }
}
