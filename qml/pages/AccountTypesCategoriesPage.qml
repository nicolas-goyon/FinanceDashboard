pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
//
// Built from the shared qml/components/ (PageHeader, PillButton,
// GhostButton, FormDialog, FormRow, ColorPickerRow, ConfirmDialog,
// EmptyState) — see ARCHITECTURE.MD § Shared Components. The chip
// edit/remove glyphs here used to be bare Text+TapHandler with no hover
// feedback at all (unlike every other Edit/Remove button in the app);
// GhostButton gives them the same hover affordance as everywhere else.

Item {
    id: page

    signal closeRequested()

    readonly property color defaultNewColor: "#89b4fa"

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
                title: "Account Types & Categories"
                onBackClicked: page.closeRequested()

                PillButton {
                    text: "＋  Add type"
                    variant: "primary"
                    onClicked: typeDialog.openFor(-1, "")
                }
                PillButton {
                    text: "＋  Add category"
                    variant: "primary"
                    onClicked: categoryDialog.openFor(-1, "", page.defaultNewColor)
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

                    EmptyState {
                        visible: AccountController.accountTypes.length === 0
                        message: "No account types yet — add one above."
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
                                    spacing: 6
                                    Text { text: typeChip.modelData.name; font.pixelSize: 14; color: "#cdd6f4" }
                                    GhostButton {
                                        text: "✎"
                                        textColor: "#89b4fa"
                                        horizontalPadding: 5
                                        onClicked: typeDialog.openFor(typeChip.index, typeChip.modelData.name)
                                    }
                                    GhostButton {
                                        text: "×"
                                        variant: "danger"
                                        fontSize: 15
                                        bold: true
                                        horizontalPadding: 5
                                        onClicked: deleteTypeDialog.openFor(typeChip.index, typeChip.modelData.name)
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

                    EmptyState {
                        visible: AccountController.accountCategories.length === 0
                        message: "No account categories yet — add one above."
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
                                    spacing: 6
                                    Rectangle {
                                        Layout.preferredWidth: 14; Layout.preferredHeight: 14; radius: 7
                                        color: catChip.modelData.color
                                        border.color: "#313244"
                                    }
                                    Text { text: catChip.modelData.name; font.pixelSize: 14; color: "#cdd6f4" }
                                    GhostButton {
                                        text: "✎"
                                        textColor: "#89b4fa"
                                        horizontalPadding: 5
                                        onClicked: categoryDialog.openFor(catChip.index, catChip.modelData.name, catChip.modelData.color)
                                    }
                                    GhostButton {
                                        text: "×"
                                        variant: "danger"
                                        fontSize: 15
                                        bold: true
                                        horizontalPadding: 5
                                        onClicked: deleteCategoryDialog.openFor(catChip.index, catChip.modelData.name)
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

    // ── Add / edit type dialog ────────────────────────────────────────────────
    FormDialog {
        id: typeDialog
        dialogTitle: typeDialog.editIndex < 0 ? "New account type" : "Edit account type"
        saveText: typeDialog.editIndex < 0 ? "Add" : "Save"
        width: 360

        property int editIndex: -1

        function openFor(idx, name) { editIndex = idx; typeNameRow.text = name; open() }

        FormRow { id: typeNameRow; label: "Name"; labelWidth: 50; placeholderText: "e.g. PEA" }

        onSaveClicked: {
            var trimmed = typeNameRow.text.trim()
            if (trimmed.length === 0)
                return
            if (editIndex < 0) {
                AccountController.addAccountType(trimmed)
            } else {
                AccountController.renameAccountType(editIndex, trimmed)
            }
            close()
        }
    }

    // ── Delete type confirmation ──────────────────────────────────────────────
    ConfirmDialog {
        id: deleteTypeDialog
        dialogTitle: "Delete account type?"
        confirmText: "Delete"

        property int typeIndex: -1

        function openFor(idx, name) {
            typeIndex = idx
            message = "This removes “" + name + "” from the list. Accounts already using "
                + "it keep the name as plain text. This cannot be undone."
            open()
        }

        onConfirmed: AccountController.removeAccountType(deleteTypeDialog.typeIndex)
    }

    // ── Add / edit category dialog ──────────────────────────────────────────
    FormDialog {
        id: categoryDialog
        dialogTitle: categoryDialog.editIndex < 0 ? "New account category" : "Edit account category"
        saveText: categoryDialog.editIndex < 0 ? "Add" : "Save"

        property int editIndex: -1

        function openFor(idx, name, color) {
            editIndex = idx
            catNameRow.text = name
            colorRow.selectedColor = color
            open()
        }

        FormRow { id: catNameRow; label: "Name"; labelWidth: 50; placeholderText: "e.g. Urgent savings" }

        ColumnLayout {
            spacing: 8
            Text { text: "Color"; font.pixelSize: 15; color: "#6c7086" }
            ColorPickerRow { id: colorRow; dialogTitle: "Choose category color" }
        }

        onSaveClicked: {
            var trimmed = catNameRow.text.trim()
            if (trimmed.length === 0)
                return
            if (editIndex < 0) {
                AccountController.addAccountCategory(trimmed, colorRow.selectedColor)
            } else {
                AccountController.renameAccountCategory(editIndex, trimmed)
                AccountController.setAccountCategoryColor(editIndex, colorRow.selectedColor)
            }
            close()
        }
    }

    // ── Delete category confirmation ────────────────────────────────────────
    ConfirmDialog {
        id: deleteCategoryDialog
        dialogTitle: "Delete account category?"
        confirmText: "Delete"

        property int categoryIndex: -1

        function openFor(idx, name) {
            categoryIndex = idx
            message = "This removes “" + name + "” from the list. Accounts already using it "
                + "keep the name as plain text. This cannot be undone."
            open()
        }

        onConfirmed: AccountController.removeAccountCategory(deleteCategoryDialog.categoryIndex)
    }
}
