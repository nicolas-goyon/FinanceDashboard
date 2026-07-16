import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import FinanceDashboard

// ── Account Types & Categories — Account Management → "Account types & categories" ──
// Manages the two classification lists accounts pick from (see
// models/Account.h / ARCHITECTURE.MD § Account Management):
//   - Account types (CCP, Livret A, Assurance Vie, …) — plain name, no color.
//   - Account categories (Day-to-day, Savings, Investments, Urgent savings, …)
//     — name + color (used for badges on AccountsManagePage.qml /
//     TransactionsManagementPage.qml).
// Moved out of AccountsManagePage.qml so that page only deals with accounts
// themselves, not the lists they're classified by — same separation as
// CategoriesManagePage.qml (transaction categories) vs. this page (account
// types/categories); unrelated lists despite the similar CRUD shape.
// Backed by AccountController (QML_SINGLETON) — same instance
// AccountsManagePage.qml reads the lists from when populating its pickers.

Item {
    id: page

    signal closeRequested()

    readonly property color defaultNewColor: "#89b4fa"

    // color -> "#rrggbb" — same helper as CategoriesManagePage.qml (avoids
    // relying on the undocumented format of color.toString()).
    function colorToHex(c) {
        function channel(v) {
            var h = Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16)
            return h.length === 1 ? "0" + h : h
        }
        return "#" + channel(c.r) + channel(c.g) + channel(c.b)
    }

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
                    text: "Account Types & Categories"
                    font.pixelSize: 23
                    font.bold: true
                    color: "#cdd6f4"
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 130; height: 30; radius: 4
                    color: addTypeHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text { anchors.centerIn: parent; text: "＋  Add type"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                    HoverHandler { id: addTypeHover }
                    TapHandler { onTapped: typeDialog.openFor(-1, "") }
                }
                Rectangle {
                    width: 170; height: 30; radius: 4
                    color: addCatHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text { anchors.centerIn: parent; text: "＋  Add category"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                    HoverHandler { id: addCatHover }
                    TapHandler { onTapped: categoryDialog.openFor(-1, "", page.defaultNewColor) }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Types are the actual banking/financial products an account can be (CCP, Livret A, "
                    + "Assurance Vie, …). Categories are the usage bucket (Day-to-day, Savings, Investments, "
                    + "Urgent savings, …) and drive the badge color shown on account rows. Both are picked "
                    + "per-account on Manage Accounts."
                font.pixelSize: 14
                color: "#6c7086"
            }

            // ── Account types ─────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: typesCol.implicitHeight + 50
                title: "Account types (" + AccountController.accountTypes.length + ")"

                ColumnLayout {
                    id: typesCol
                    anchors.fill: parent
                    spacing: 10

                    Text {
                        visible: AccountController.accountTypes.length === 0
                        text: "No account types yet — add one above."
                        font.pixelSize: 15
                        color: "#6c7086"
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 10

                        Repeater {
                            model: AccountController.accountTypes
                            delegate: Rectangle {
                                id: typeChip
                                required property var modelData
                                required property int index
                                width: typeChipRow.implicitWidth + 20
                                height: 32
                                radius: 16
                                color: "#181825"
                                border.color: "#313244"

                                RowLayout {
                                    id: typeChipRow
                                    anchors.centerIn: parent
                                    spacing: 8
                                    Text { text: typeChip.modelData.name; font.pixelSize: 14; color: "#cdd6f4" }
                                    Text {
                                        text: "✎"
                                        font.pixelSize: 13
                                        color: "#89b4fa"
                                        TapHandler { onTapped: typeDialog.openFor(typeChip.index, typeChip.modelData.name) }
                                    }
                                    Text {
                                        text: "×"
                                        font.pixelSize: 15
                                        font.bold: true
                                        color: "#f38ba8"
                                        TapHandler { onTapped: deleteTypeDialog.openFor(typeChip.index, typeChip.modelData.name) }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Account categories ───────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: catsCol.implicitHeight + 50
                title: "Account categories (" + AccountController.accountCategories.length + ")"

                ColumnLayout {
                    id: catsCol
                    anchors.fill: parent
                    spacing: 10

                    Text {
                        visible: AccountController.accountCategories.length === 0
                        text: "No account categories yet — add one above."
                        font.pixelSize: 15
                        color: "#6c7086"
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 10

                        Repeater {
                            model: AccountController.accountCategories
                            delegate: Rectangle {
                                id: catChip
                                required property var modelData
                                required property int index
                                width: chipRow.implicitWidth + 20
                                height: 32
                                radius: 16
                                color: "#181825"
                                border.color: "#313244"

                                RowLayout {
                                    id: chipRow
                                    anchors.centerIn: parent
                                    spacing: 8
                                    Rectangle {
                                        width: 14; height: 14; radius: 7
                                        color: catChip.modelData.color
                                        border.color: "#313244"
                                    }
                                    Text { text: catChip.modelData.name; font.pixelSize: 14; color: "#cdd6f4" }
                                    Text {
                                        text: "✎"
                                        font.pixelSize: 13
                                        color: "#89b4fa"
                                        TapHandler {
                                            onTapped: categoryDialog.openFor(catChip.index, catChip.modelData.name, catChip.modelData.color)
                                        }
                                    }
                                    Text {
                                        text: "×"
                                        font.pixelSize: 15
                                        font.bold: true
                                        color: "#f38ba8"
                                        TapHandler {
                                            onTapped: deleteCategoryDialog.openFor(catChip.index, catChip.modelData.name)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }

    // ── Add / edit type dialog ────────────────────────────────────────────────
    Popup {
        id: typeDialog
        anchors.centerIn: parent
        width: 360
        padding: 20
        modal: true

        property int editIndex: -1

        function openFor(idx, name) { editIndex = idx; typeNameField.text = name; open() }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: typeDialog.editIndex < 0 ? "New account type" : "Edit account type"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }
            RowLayout {
                spacing: 10
                Text { Layout.preferredWidth: 50; text: "Name"; font.pixelSize: 15; color: "#6c7086" }
                TextField {
                    id: typeNameField
                    Layout.fillWidth: true
                    placeholderText: "e.g. PEA"
                    placeholderTextColor: "#45475a"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    background: Rectangle { color: "#11111b"; radius: 4; border.color: typeNameField.activeFocus ? "#89b4fa" : "#313244" }
                }
            }
            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: typeCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: typeCancelHover }
                    TapHandler { onTapped: typeDialog.close() }
                }
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: typeSaveHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text {
                        anchors.centerIn: parent
                        text: typeDialog.editIndex < 0 ? "Add" : "Save"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#cdd6f4"
                    }
                    HoverHandler { id: typeSaveHover }
                    TapHandler {
                        onTapped: {
                            var trimmed = typeNameField.text.trim()
                            if (trimmed.length === 0)
                                return
                            if (typeDialog.editIndex < 0) {
                                AccountController.addAccountType(trimmed)
                            } else {
                                AccountController.renameAccountType(typeDialog.editIndex, trimmed)
                            }
                            typeDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ── Delete type confirmation ──────────────────────────────────────────────
    Popup {
        id: deleteTypeDialog
        anchors.centerIn: parent
        width: 340
        padding: 20
        modal: true

        property int typeIndex: -1
        property string typeName: ""

        function openFor(idx, name) { typeIndex = idx; typeName = name; open() }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Delete account type?"; font.pixelSize: 19; font.bold: true; color: "#cdd6f4" }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "This removes “" + deleteTypeDialog.typeName + "” from the list. Accounts already using "
                    + "it keep the name as plain text. This cannot be undone."
                font.pixelSize: 14
                color: "#a6adc8"
            }
            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: delTypeCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: delTypeCancelHover }
                    TapHandler { onTapped: deleteTypeDialog.close() }
                }
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: delTypeConfirmHover.hovered ? "#5a2d36" : "#3d2028"
                    border.color: "#f38ba8"
                    Text { anchors.centerIn: parent; text: "Delete"; font.pixelSize: 15; font.bold: true; color: "#f38ba8" }
                    HoverHandler { id: delTypeConfirmHover }
                    TapHandler {
                        onTapped: {
                            AccountController.removeAccountType(deleteTypeDialog.typeIndex)
                            deleteTypeDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ── Add / edit category dialog ──────────────────────────────────────────
    Popup {
        id: categoryDialog
        anchors.centerIn: parent
        width: 380
        padding: 20
        modal: true

        property int editIndex: -1
        property string selectedColor: "#89b4fa"

        function openFor(idx, name, color) {
            editIndex = idx
            catNameField.text = name
            selectedColor = color
            open()
        }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: categoryDialog.editIndex < 0 ? "New account category" : "Edit account category"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }

            RowLayout {
                spacing: 10
                Text { Layout.preferredWidth: 50; text: "Name"; font.pixelSize: 15; color: "#6c7086" }
                TextField {
                    id: catNameField
                    Layout.fillWidth: true
                    placeholderText: "e.g. Urgent savings"
                    placeholderTextColor: "#45475a"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    background: Rectangle {
                        color: "#11111b"
                        radius: 4
                        border.color: catNameField.activeFocus ? "#89b4fa" : "#313244"
                    }
                }
            }

            Text { text: "Color"; font.pixelSize: 15; color: "#6c7086" }
            RowLayout {
                spacing: 10
                Rectangle {
                    width: 32; height: 32; radius: 16
                    color: categoryDialog.selectedColor
                    border.width: 2
                    border.color: "#45475a"
                    TapHandler {
                        onTapped: {
                            categoryColorDialog.selectedColor = categoryDialog.selectedColor
                            categoryColorDialog.open()
                        }
                    }
                }
                Rectangle {
                    width: 110; height: 26; radius: 4
                    color: catColorHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Choose color…"; font.pixelSize: 14; color: "#cdd6f4" }
                    HoverHandler { id: catColorHover }
                    TapHandler {
                        onTapped: {
                            categoryColorDialog.selectedColor = categoryDialog.selectedColor
                            categoryColorDialog.open()
                        }
                    }
                }
                Text { text: categoryDialog.selectedColor; font.pixelSize: 14; color: "#6c7086" }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: catCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: catCancelHover }
                    TapHandler { onTapped: categoryDialog.close() }
                }
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: catSaveHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text {
                        anchors.centerIn: parent
                        text: categoryDialog.editIndex < 0 ? "Add" : "Save"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#cdd6f4"
                    }
                    HoverHandler { id: catSaveHover }
                    TapHandler {
                        onTapped: {
                            var trimmed = catNameField.text.trim()
                            if (trimmed.length === 0)
                                return
                            if (categoryDialog.editIndex < 0) {
                                AccountController.addAccountCategory(trimmed, categoryDialog.selectedColor)
                            } else {
                                AccountController.renameAccountCategory(categoryDialog.editIndex, trimmed)
                                AccountController.setAccountCategoryColor(categoryDialog.editIndex, categoryDialog.selectedColor)
                            }
                            categoryDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ── Delete category confirmation ────────────────────────────────────────
    Popup {
        id: deleteCategoryDialog
        anchors.centerIn: parent
        width: 340
        padding: 20
        modal: true

        property int categoryIndex: -1
        property string categoryName: ""

        function openFor(idx, name) { categoryIndex = idx; categoryName = name; open() }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Delete account category?"; font.pixelSize: 19; font.bold: true; color: "#cdd6f4" }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "This removes “" + deleteCategoryDialog.categoryName + "” from the list. Accounts "
                    + "already using it keep the name as plain text. This cannot be undone."
                font.pixelSize: 14
                color: "#a6adc8"
            }
            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: delCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: delCancelHover }
                    TapHandler { onTapped: deleteCategoryDialog.close() }
                }
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: delConfirmHover.hovered ? "#5a2d36" : "#3d2028"
                    border.color: "#f38ba8"
                    Text { anchors.centerIn: parent; text: "Delete"; font.pixelSize: 15; font.bold: true; color: "#f38ba8" }
                    HoverHandler { id: delConfirmHover }
                    TapHandler {
                        onTapped: {
                            AccountController.removeAccountCategory(deleteCategoryDialog.categoryIndex)
                            deleteCategoryDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ── Native color picker (categories only — types have no color) ──────────
    ColorDialog {
        id: categoryColorDialog
        title: "Choose category color"
        onAccepted: categoryDialog.selectedColor = page.colorToHex(selectedColor)
    }
}
