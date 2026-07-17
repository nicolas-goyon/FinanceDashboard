pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Categories Management — Transactions Management → "Manage Categories" ────
// Add/rename/recolor/remove categories and their subcategories. Backed by
// CategoryController (QML_SINGLETON, src/controllers/dataentry/
// CategoryController.h) — the same shared instance read by the import
// review table (ImportSnapshotPage.qml), so edits made here show up
// immediately there too.
// DB-backed (categories/subcategories tables — see CategoryController.h and
// ARCHITECTURE.MD § Category Management): every add/rename/recolor/remove
// here writes through to SQLite immediately, so deletions are permanent
// across restarts — hence the confirmation prompt before removing a
// category. Replaces the old keyword auto-suggestion system, removed per
// product decision — categories/subcategories are now a user-managed list
// picked manually everywhere.
//
// Built from the shared qml/components/ (PageHeader, GhostButton,
// FormDialog, FormRow, ColorPickerRow, ConfirmDialog, EmptyState) — see
// ARCHITECTURE.MD § Shared Components.

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
                title: "Manage Categories"
                onBackClicked: page.closeRequested()

                StatusBadge {
                    visible: CategoryController.statusText.length > 0
                    text: CategoryController.statusText
                    variant: "success"
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
                text: "Categories and subcategories used everywhere a transaction gets classified "
                    + "(statement import). Colors are just for legends/swatches — no charts read "
                    + "them yet."
                font.pixelSize: 14
                color: "#6c7086"
            }

            // ── Category list ────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: catsCol.implicitHeight + 50
                title: "Categories (" + CategoryController.categories.length + ")"

                ColumnLayout {
                    id: catsCol
                    anchors.fill: parent
                    spacing: 14

                    EmptyState {
                        visible: CategoryController.categories.length === 0
                        message: "No categories yet — add one above."
                    }

                    Repeater {
                        model: CategoryController.categories
                        delegate: ColumnLayout {
                            id: catRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 6

                            // Category header row
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 18; Layout.preferredHeight: 18; radius: 9
                                    color: catRow.modelData.color
                                    border.color: "#313244"
                                }
                                Text {
                                    text: catRow.modelData.name
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: "#cdd6f4"
                                }
                                Text {
                                    text: "(" + catRow.modelData.subcategories.length + " subcategor"
                                        + (catRow.modelData.subcategories.length === 1 ? "y" : "ies") + ")"
                                    font.pixelSize: 13
                                    color: "#6c7086"
                                }
                                Item { Layout.fillWidth: true }

                                GhostButton {
                                    text: "✎ Edit"
                                    textColor: "#89b4fa"
                                    onClicked: categoryDialog.openFor(catRow.index, catRow.modelData.name, catRow.modelData.color)
                                }
                                GhostButton {
                                    text: "＋ Subcategory"
                                    textColor: "#a6e3a1"
                                    onClicked: subcategoryDialog.openFor(
                                        catRow.index, -1, "",
                                        CategoryController.suggestSubcategoryColor(catRow.index))
                                }
                                GhostButton {
                                    text: "×"
                                    variant: "danger"
                                    fontSize: 17
                                    bold: true
                                    horizontalPadding: 6
                                    onClicked: deleteCategoryDialog.openFor(catRow.index, catRow.modelData.name)
                                }
                            }

                            // Subcategory rows (indented)
                            Repeater {
                                model: catRow.modelData.subcategories
                                delegate: RowLayout {
                                    id: subRow
                                    required property var modelData
                                    required property int index
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 28
                                    spacing: 10

                                    Rectangle {
                                        Layout.preferredWidth: 14; Layout.preferredHeight: 14; radius: 7
                                        color: subRow.modelData.color
                                        border.color: "#313244"
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: subRow.modelData.name
                                        font.pixelSize: 15
                                        color: "#a6adc8"
                                    }
                                    GhostButton {
                                        text: "✎ Edit"
                                        textColor: "#89b4fa"
                                        fontSize: 12
                                        onClicked: subcategoryDialog.openFor(catRow.index, subRow.index, subRow.modelData.name, subRow.modelData.color)
                                    }
                                    GhostButton {
                                        text: "×"
                                        variant: "danger"
                                        fontSize: 15
                                        bold: true
                                        horizontalPadding: 5
                                        onClicked: CategoryController.removeSubcategory(catRow.index, subRow.index)
                                    }
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

    // ── Add / edit category dialog ──────────────────────────────────────────
    FormDialog {
        id: categoryDialog
        dialogTitle: categoryDialog.editIndex < 0 ? "New category" : "Edit category"
        saveText: categoryDialog.editIndex < 0 ? "Add" : "Save"

        property int editIndex: -1

        function openFor(idx, name, color) {
            editIndex = idx
            nameRow.text = name
            colorRow.selectedColor = color
            open()
        }

        FormRow { id: nameRow; label: "Name"; placeholderText: "e.g. Health" }

        ColumnLayout {
            spacing: 8
            Text { text: "Color"; font.pixelSize: 15; color: "#6c7086" }
            ColorPickerRow { id: colorRow; dialogTitle: "Choose category color" }
        }

        onSaveClicked: {
            var trimmed = nameRow.text.trim()
            if (trimmed.length === 0)
                return
            if (editIndex < 0) {
                CategoryController.addCategory(trimmed, colorRow.selectedColor)
            } else {
                CategoryController.renameCategory(editIndex, trimmed)
                CategoryController.setCategoryColor(editIndex, colorRow.selectedColor)
            }
            close()
        }
    }

    // ── Add / edit subcategory dialog ───────────────────────────────────────
    FormDialog {
        id: subcategoryDialog
        dialogTitle: subcategoryDialog.editSubIndex < 0 ? "New subcategory" : "Edit subcategory"
        saveText: subcategoryDialog.editSubIndex < 0 ? "Add" : "Save"

        property int parentCategoryIndex: -1
        property int editSubIndex: -1

        function openFor(catIndex, subIndex, name, color) {
            parentCategoryIndex = catIndex
            editSubIndex = subIndex
            subNameRow.text = name
            subColorRow.selectedColor = color
            open()
        }

        FormRow { id: subNameRow; label: "Name"; placeholderText: "e.g. Pharmacy" }

        ColumnLayout {
            spacing: 8
            Text { text: "Color"; font.pixelSize: 15; color: "#6c7086" }
            ColorPickerRow { id: subColorRow; dialogTitle: "Choose subcategory color" }
        }

        onSaveClicked: {
            var trimmed = subNameRow.text.trim()
            if (trimmed.length === 0 || parentCategoryIndex < 0)
                return
            if (editSubIndex < 0) {
                CategoryController.addSubcategory(parentCategoryIndex, trimmed, subColorRow.selectedColor)
            } else {
                CategoryController.renameSubcategory(parentCategoryIndex, editSubIndex, trimmed)
                CategoryController.setSubcategoryColor(parentCategoryIndex, editSubIndex, subColorRow.selectedColor)
            }
            close()
        }
    }

    // ── Delete category confirmation ────────────────────────────────────────
    // Deleting a category also drops all of its subcategories, so this asks
    // for an explicit confirmation before calling
    // CategoryController.removeCategory() (per product request — no
    // undo/history for category edits, see file header).
    ConfirmDialog {
        id: deleteCategoryDialog
        dialogTitle: "Delete category?"
        confirmText: "Delete"

        property int categoryIndex: -1

        function openFor(idx, name) {
            categoryIndex = idx
            message = "This will remove “" + name + "” and all of its subcategories. This cannot be undone."
            open()
        }

        onConfirmed: CategoryController.removeCategory(deleteCategoryDialog.categoryIndex)
    }
}
