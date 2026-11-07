pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── AccountFormDialog ────────────────────────────────────────────────────────
// The form of an account: name, type, category, currency, notes, net worth
// (#206). openForNew() adds an account (accountAdded(index) then), and
// openForEdit(index) changes one. Used by the accounts page and by the import
// review ("＋ New account" when no account fits the statement).
FormDialog {
    id: accountDialog
    dialogTitle: accountDialog.editIndex < 0 ? qsTr("New account") : qsTr("Edit account")
    saveText: accountDialog.editIndex < 0 ? qsTr("Add") : qsTr("Save")
    width: 440

    property int editIndex: -1

    // A new account was added: its index in AccountController.accounts (the
    // import review selects it).
    signal accountAdded(int index)
    // The stored values of the edited account. A value that is not in its
    // list (a removed type or category, an unknown currency) gives the
    // ComboBox index -1: the box shows the value, and Save keeps it (#63).
    property string keptType: ""
    property string keptCategory: ""
    property string keptCurrency: ""

    function keptText(value) {
        return value.length > 0 ? qsTr("%1 (kept)").arg(value) : qsTr("(none)")
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

    // `name`: a name to start from (optional).
    function openForNew(name) {
        editIndex = -1
        keptType = ""
        keptCategory = ""
        keptCurrency = ""
        accNameRow.text = name !== undefined ? name : ""
        // -1 when there is no type, after "Reset Database" (#62).
        accTypeBox.currentIndex = AccountController.accountTypes.length > 0 ? 0 : -1
        accCategoryBox.currentIndex = -1
        accCurrencyBox.currentIndex = 0
        accNotesRow.text = ""
        inNetWorthBox.checked = true
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
        inNetWorthBox.checked = acc.inNetWorth
        open()
    }

    FormRow { id: accNameRow; label: qsTr("Name"); labelWidth: 70; placeholderText: qsTr("e.g. Livret Jeune") }
    RowLayout {
        spacing: 10
        Text { Layout.preferredWidth: 70; text: qsTr("Type"); font.pixelSize: 15; color: Theme.overlay0 }
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
        text: qsTr("There is no account type. The account is saved without a type. To add types, go to "
            + "Account Management → \"Account types & categories\".")
        font.pixelSize: 13
        color: Theme.yellow
    }
    RowLayout {
        spacing: 10
        Text { Layout.preferredWidth: 70; text: qsTr("Category"); font.pixelSize: 15; color: Theme.overlay0 }
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
        Text { Layout.preferredWidth: 70; text: qsTr("Currency"); font.pixelSize: 15; color: Theme.overlay0 }
        ComboBox {
            id: accCurrencyBox
            Layout.preferredWidth: 140
            model: accountDialog.currencyOptions
            textRole: "label"
            displayText: currentIndex >= 0 ? currentText : accountDialog.keptText(accountDialog.keptCurrency)
        }
    }
    FormRow { id: accNotesRow; label: qsTr("Notes"); labelWidth: 70; placeholderText: qsTr("optional") }
    // #206: a move between two accounts of the net worth is neither
    // income nor spending.
    CheckBox {
        id: inNetWorthBox
        checked: true
        text: qsTr("Part of my net worth")
        palette.windowText: Theme.text
    }
    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("Turn it off for an account that you only follow (a joint account, the account of a "
            + "child): the money that goes to it then counts as spending.")
        font.pixelSize: 12
        color: Theme.overlay0
    }

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
            var before = AccountController.accounts.length
            AccountController.addAccount(trimmed, type, category, currency, accNotesRow.text.trim())
            if (AccountController.accounts.length > before) {
                var added = AccountController.accounts.length - 1
                AccountController.setInNetWorth(added, inNetWorthBox.checked)
                accountDialog.accountAdded(added)
            }
        } else {
            AccountController.updateAccount(editIndex, trimmed, type, category, currency, accNotesRow.text.trim())
            AccountController.setInNetWorth(editIndex, inNetWorthBox.checked)
        }
        close()
    }
}
