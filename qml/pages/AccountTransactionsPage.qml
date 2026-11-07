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
                title: page.account !== null ? qsTr("Transactions — %1").arg(page.account.name) : qsTr("Transactions")
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

                Text { text: qsTr("%1 row(s)").arg(page.rows.length); font.pixelSize: 14; color: Theme.overlay0 }
            }

            Text {
                visible: AccountController.accounts.length === 0
                text: qsTr("No accounts yet — add one from Account Management → Manage Accounts.")
                font.pixelSize: 15
                color: Theme.red
            }

            Text {
                visible: AccountController.accounts.length > 0 && page.account === null
                text: qsTr("This account is no longer available.")
                font.pixelSize: 15
                color: Theme.red
            }

            // ── Table ─────────────────────────────────────────────────────────
            ChartCard {
                visible: page.account !== null
                Layout.fillWidth: true
                implicitHeight: txCol.implicitHeight + 50
                title: qsTr("All transactions")

                ColumnLayout {
                    id: txCol
                    anchors.fill: parent
                    spacing: 0

                    // The columns need more width than the card has at the default window
                    // size (1280 px), so the header and the rows scroll horizontally (#61).
                    // The rows fill the card when it is wider than the columns.
                    // TableScrollView: a vertical wheel over the table scrolls the page.
                    TableScrollView {
                        id: txScroll
                        Layout.fillWidth: true
                        // The layout gives the width, and the table scrolls in it. The
                        // default preferred width is the implicit width, which comes from the
                        // table width: that is a binding loop.
                        Layout.preferredWidth: 0
                        Layout.preferredHeight: txTable.implicitHeight
                            + (txScroll.ScrollBar.horizontal.visible ? txScroll.ScrollBar.horizontal.height : 0)
                        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                        // Always shown when the table is wider than the card: with AsNeeded, the
                        // Basic style shows the bar only while the pointer is on it, and nothing
                        // tells that the table scrolls.
                        ScrollBar.horizontal.policy: txTable.columnsWidth > txScroll.availableWidth
                            ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                        contentWidth: txTable.width
                        contentHeight: txTable.implicitHeight

                        ColumnLayout {
                            id: txTable
                            // The width of the card, or the width of the columns when it is more:
                            // the 9 columns (90 + 260 + 44 + 90 + 90 + 100 + 130 + 130 + 60 = 994),
                            // 9 spacings of 8 between the 10 items of a row (with the spacer), and
                            // the 10 + 10 margins of the row. Keep it equal to the widths below.
                            readonly property real columnsWidth: 994 + 9 * 8 + 20
                            // Delayed: the width changes the implicit height of this layout, that
                            // height changes the layout of the card, and that layout gives
                            // availableWidth. Without the delay, that is a binding loop.
                            Binding on width { value: Math.max(txScroll.availableWidth, txTable.columnsWidth); delayed: true }
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                color: Theme.mantle
                                radius: 4
                                RowLayout {
                                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                    spacing: 8
                                    Text { Layout.preferredWidth: 90;  text: qsTr("DATE");        font.pixelSize: 13; font.bold: true; color: Theme.overlay0 }
                                    Text { Layout.preferredWidth: 260; text: qsTr("NAME");        font.pixelSize: 13; font.bold: true; color: Theme.overlay0 }
                                    // Narrow column — rows only show a view-icon here (see
                                    // descriptionPopup below); the fillWidth spacer after it takes up
                                    // the space the old fillWidth DESCRIPTION text column used to, so
                                    // DEBIT/CREDIT/… stay pinned to the right — same pattern as
                                    // ImportSnapshotPage.qml's transaction table.
                                    Text { Layout.preferredWidth: 44;  text: qsTr("DESC.");       font.pixelSize: 13; font.bold: true; color: Theme.overlay0 }
                                    Item { Layout.fillWidth: true }
                                    Text { Layout.preferredWidth: 90;  text: qsTr("DEBIT");       font.pixelSize: 13; font.bold: true; color: Theme.overlay0; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 90;  text: qsTr("CREDIT");      font.pixelSize: 13; font.bold: true; color: Theme.overlay0; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 100; text: qsTr("BALANCE");     font.pixelSize: 13; font.bold: true; color: Theme.overlay0; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 130; text: qsTr("CATEGORY");    font.pixelSize: 13; font.bold: true; color: Theme.overlay0 }
                                    Text { Layout.preferredWidth: 130; text: qsTr("SUBCATEGORY"); font.pixelSize: 13; font.bold: true; color: Theme.overlay0 }
                                    Text { Layout.preferredWidth: 60;  text: "";            font.pixelSize: 13; font.bold: true; color: Theme.overlay0 }
                                }
                            }

                            EmptyState {
                                visible: page.rows.length === 0
                                Layout.topMargin: 10
                                message: qsTr("No transactions saved for this account yet.")
                            }

                            Repeater {
                                model: page.rows
                                delegate: Rectangle {
                                    id: txRow
                                    required property var modelData
                                    required property int index
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 36
                                    color: index % 2 === 0 ? "transparent" : Theme.mantle

                                    RowLayout {
                                        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                        spacing: 8
                                        Text { Layout.preferredWidth: 90; text: txRow.modelData.operationDate; font.pixelSize: 14; color: Theme.overlay0 }
                                        Text {
                                            Layout.preferredWidth: 260
                                            text: txRow.modelData.name.length > 0 ? txRow.modelData.name : "—"
                                            font.pixelSize: 15
                                            color: Theme.text
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
                                        Text { Layout.preferredWidth: 90;  text: txRow.modelData.debitText;  font.pixelSize: 15; color: Theme.red; horizontalAlignment: Text.AlignRight }
                                        Text { Layout.preferredWidth: 90;  text: txRow.modelData.creditText; font.pixelSize: 15; color: Theme.green; horizontalAlignment: Text.AlignRight }
                                        Text { Layout.preferredWidth: 100; text: txRow.modelData.balanceText; font.pixelSize: 14; color: Theme.overlay0; horizontalAlignment: Text.AlignRight }

                                        // Category/subcategory badges — same "colored pill" language
                                        // as the account-category badge on AccountsManagePage.qml,
                                        // colors resolved via CategoryController (see that
                                        // singleton's categoryColor()/subcategoryColor() convenience
                                        // lookups — ARCHITECTURE.MD § Category Management).
                                        // #207: an internal transfer has no spending category.
                                        StatusBadge {
                                            Layout.preferredWidth: 130
                                            visible: txRow.modelData.internalTransfer
                                            pillHeight: 20
                                            variant: "success"
                                            text: qsTr("⇄ Internal transfer")
                                        }
                                        Rectangle {
                                            Layout.preferredWidth: 130
                                            visible: !txRow.modelData.internalTransfer && txRow.modelData.category.length > 0
                                            Layout.preferredHeight: 20
                                            radius: 10
                                            color: Qt.rgba(1, 1, 1, 0.08)
                                            border.color: CategoryController.categoryColor(txRow.modelData.category) || Theme.overlay0
                                            Text {
                                                anchors.centerIn: parent
                                                text: txRow.modelData.category
                                                font.pixelSize: 12
                                                color: CategoryController.categoryColor(txRow.modelData.category) || Theme.overlay0
                                                elide: Text.ElideRight
                                                width: Math.min(implicitWidth, 118)
                                            }
                                        }
                                        Text { Layout.preferredWidth: 130; visible: !txRow.modelData.internalTransfer && txRow.modelData.category.length === 0; text: "—"; font.pixelSize: 14; color: Theme.overlay0 }

                                        Rectangle {
                                            Layout.preferredWidth: 130
                                            visible: txRow.modelData.subcategory.length > 0
                                            Layout.preferredHeight: 20
                                            radius: 10
                                            color: Qt.rgba(1, 1, 1, 0.08)
                                            border.color: CategoryController.subcategoryColor(txRow.modelData.category, txRow.modelData.subcategory) || Theme.overlay0
                                            Text {
                                                anchors.centerIn: parent
                                                text: txRow.modelData.subcategory
                                                font.pixelSize: 12
                                                color: CategoryController.subcategoryColor(txRow.modelData.category, txRow.modelData.subcategory) || Theme.overlay0
                                                elide: Text.ElideRight
                                                width: Math.min(implicitWidth, 118)
                                            }
                                        }
                                        Text { Layout.preferredWidth: 130; visible: txRow.modelData.subcategory.length === 0; text: "—"; font.pixelSize: 14; color: Theme.overlay0 }

                                        GhostButton {
                                            Layout.preferredWidth: 60
                                            text: qsTr("✎ Edit")
                                            textColor: Theme.blue
                                            onClicked: editDialog.openFor(txRow.modelData)
                                        }
                                    }
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
            color: Theme.mantle
            border.color: Theme.surface1
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text { text: qsTr("Full description"); font.pixelSize: 18; font.bold: true; color: Theme.text }

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
                    color: Theme.text
                    selectionColor: Theme.blue
                    selectedTextColor: Theme.crust
                }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                PillButton { text: qsTr("Close"); variant: "neutral"; onClicked: descriptionPopup.close() }
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
        dialogTitle: qsTr("Edit transaction")
        saveText: qsTr("Save")
        width: 460

        property int transactionId: -1
        property bool wasTransfer: false

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
            editTransferBox.checked = row.internalTransfer
            editDialog.wasTransfer = row.internalTransfer

            open()
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Balance shown in the table is not recalculated after an edit — fix debit/credit "
                + "here for categorization/bookkeeping, not to correct the bank's own running balance.")
            font.pixelSize: 12
            color: Theme.overlay0
        }
        FormRow { id: editConfirmationDateRow; label: qsTr("Confirm. date"); labelWidth: 110; placeholderText: qsTr("yyyy-MM-dd") }
        FormRow { id: editOperationDateRow; label: qsTr("Operation date"); labelWidth: 110; placeholderText: qsTr("yyyy-MM-dd") }
        FormRow { id: editNameRow; label: qsTr("Name"); labelWidth: 110 }
        FormRow { id: editDescriptionRow; label: qsTr("Description"); labelWidth: 110 }
        FormRow { id: editDebitRow; label: qsTr("Debit"); labelWidth: 110; placeholderText: "0.00" }
        FormRow { id: editCreditRow; label: qsTr("Credit"); labelWidth: 110; placeholderText: "0.00" }
        RowLayout {
            spacing: 10
            Text { Layout.preferredWidth: 110; text: qsTr("Category"); font.pixelSize: 15; color: Theme.overlay0 }
            // "＋ New category…" adds a category and selects it (#237).
            AddOptionComboBox {
                id: editCategoryBox
                Layout.fillWidth: true
                onActivated: {
                    editSubcategoryBox.model = editDialog.subcategoryNames(editCategoryBox.currentText)
                    editSubcategoryBox.currentIndex = -1
                }
                addText: qsTr("＋ New category…")
                onAddRequested: newEditOptionDialog.openForCategory()
            }
        }
        RowLayout {
            spacing: 10
            Text { Layout.preferredWidth: 110; text: qsTr("Subcategory"); font.pixelSize: 15; color: Theme.overlay0 }
            // A category with no subcategory yet: the list has only
            // "＋ New subcategory…".
            AddOptionComboBox {
                id: editSubcategoryBox
                Layout.fillWidth: true
                enabled: editCategoryBox.currentIndex >= 0
                addText: qsTr("＋ New subcategory…")
                onAddRequested: newEditOptionDialog.openForSubcategory(
                    editDialog.categoryNames().indexOf(editCategoryBox.currentText))
            }
        }
        // #207: a move between two accounts of the net worth. Off also
        // cancels the other side of the transfer.
        CheckBox {
            id: editTransferBox
            text: qsTr("Internal transfer (not income, not spending)")
            palette.windowText: Theme.text
        }

        // The dialog of "＋ New category…" and "＋ New subcategory…" (#237).
        // It adds the category (or the subcategory of the selected category)
        // and selects it. A name that is already there (CategoryController
        // refuses it, case-insensitive) selects that category or subcategory.
        NewOptionDialog {
            id: newEditOptionDialog
            withColor: true

            property int categoryIndex: -1   // -1: a new category

            function indexOfName(names, name) {
                var wanted = name.toLowerCase()
                for (var i = 0; i < names.length; ++i)
                    if (names[i].toLowerCase() === wanted)
                        return i
                return -1
            }

            function openForCategory() {
                categoryIndex = -1
                openWith(qsTr("New category"), qsTr("e.g. Health"), Theme.blue)
            }
            function openForSubcategory(catIndex) {
                if (catIndex < 0)
                    return
                categoryIndex = catIndex
                openWith(qsTr("New subcategory of “%1”").arg(CategoryController.categories[catIndex].name),
                         qsTr("e.g. Pharmacy"), CategoryController.suggestSubcategoryColor(catIndex))
            }

            onOptionAccepted: (name, color) => {
                if (categoryIndex < 0) {
                    CategoryController.addCategory(name, color)
                    var names = editDialog.categoryNames()
                    editCategoryBox.model = names
                    var ci = indexOfName(names, name)
                    editCategoryBox.currentIndex = ci
                    editSubcategoryBox.model = ci >= 0 ? editDialog.subcategoryNames(names[ci]) : []
                    editSubcategoryBox.currentIndex = -1
                } else {
                    var categoryName = CategoryController.categories[categoryIndex].name
                    CategoryController.addSubcategory(categoryIndex, name, color)
                    var subNames = editDialog.subcategoryNames(categoryName)
                    editSubcategoryBox.model = subNames
                    editSubcategoryBox.currentIndex = indexOfName(subNames, name)
                }
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
            if (ok && editTransferBox.checked !== editDialog.wasTransfer)
                ok = AccountController.setInternalTransfer(editDialog.transactionId, editTransferBox.checked)
            if (ok) {
                page.reloadRows()
                close()
            }
        }
    }
}
