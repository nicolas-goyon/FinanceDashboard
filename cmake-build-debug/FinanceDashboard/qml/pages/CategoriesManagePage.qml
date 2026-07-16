import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import FinanceDashboard

// ── Categories Management — Data Entry → "Manage Categories" ──────────────────
// Add/rename/recolor/remove categories and their subcategories. Backed by
// CategoryController (QML_SINGLETON, src/controllers/dataentry/
// CategoryController.h) — the same shared instance read by the "Add
// transaction" form (DataEntryPage.qml) and the import review table
// (ImportSnapshotPage.qml), so edits made here show up immediately on both.
// In-memory only — resets on app restart, same scope as the rest of the app
// (see ARCHITECTURE.MD § Category Management). Replaces the old keyword
// auto-suggestion system, removed per product decision — categories/
// subcategories are now a user-managed list picked manually everywhere.

Item {
    id: page

    signal closeRequested()

    readonly property color defaultNewColor: "#89b4fa"

    // color -> "#rrggbb", built from the documented r/g/b real (0..1)
    // properties of the QML color value type (avoids relying on the
    // undocumented format of color.toString()).
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
                    text: "Manage Categories"
                    font.pixelSize: 23
                    font.bold: true
                    color: "#cdd6f4"
                }
                Text { text: "[session only]"; font.pixelSize: 14; color: "#f38ba8" }
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 150; height: 30; radius: 4
                    color: addCatHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text {
                        anchors.centerIn: parent
                        text: "＋  Add category"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#cdd6f4"
                    }
                    HoverHandler { id: addCatHover }
                    TapHandler { onTapped: categoryDialog.openFor(-1, "", page.defaultNewColor) }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Categories and subcategories used everywhere a transaction gets classified "
                    + "(Add transaction, statement import). Colors are just for legends/swatches — "
                    + "no charts read them yet."
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

                    Text {
                        visible: CategoryController.categories.length === 0
                        text: "No categories yet — add one above."
                        font.pixelSize: 15
                        color: "#6c7086"
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
                                    width: 18; height: 18; radius: 9
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

                                Rectangle {
                                    width: 60; height: 22; radius: 4
                                    color: "transparent"
                                    border.color: "#313244"
                                    Text { anchors.centerIn: parent; text: "✎ Edit"; font.pixelSize: 13; color: "#89b4fa" }
                                    TapHandler {
                                        onTapped: categoryDialog.openFor(catRow.index, catRow.modelData.name, catRow.modelData.color)
                                    }
                                }
                                Rectangle {
                                    width: 112; height: 22; radius: 4
                                    color: "transparent"
                                    border.color: "#313244"
                                    Text { anchors.centerIn: parent; text: "＋ Subcategory"; font.pixelSize: 13; color: "#a6e3a1" }
                                    TapHandler {
                                        onTapped: subcategoryDialog.openFor(
                                            catRow.index, -1, "",
                                            CategoryController.suggestSubcategoryColor(catRow.index))
                                    }
                                }
                                Rectangle {
                                    width: 22; height: 22; radius: 4
                                    color: removeCatHover.hovered ? "#3d2028" : "transparent"
                                    border.color: removeCatHover.hovered ? "#f38ba8" : "#313244"
                                    Text {
                                        anchors.centerIn: parent
                                        text: "×"
                                        font.pixelSize: 17
                                        font.bold: true
                                        color: removeCatHover.hovered ? "#f38ba8" : "#6c7086"
                                    }
                                    HoverHandler { id: removeCatHover }
                                    TapHandler { onTapped: CategoryController.removeCategory(catRow.index) }
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
                                        width: 14; height: 14; radius: 7
                                        color: subRow.modelData.color
                                        border.color: "#313244"
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: subRow.modelData.name
                                        font.pixelSize: 15
                                        color: "#a6adc8"
                                    }
                                    Rectangle {
                                        width: 60; height: 20; radius: 4
                                        color: "transparent"
                                        border.color: "#313244"
                                        Text { anchors.centerIn: parent; text: "✎ Edit"; font.pixelSize: 12; color: "#89b4fa" }
                                        TapHandler {
                                            onTapped: subcategoryDialog.openFor(catRow.index, subRow.index, subRow.modelData.name, subRow.modelData.color)
                                        }
                                    }
                                    Rectangle {
                                        width: 20; height: 20; radius: 4
                                        color: removeSubHover.hovered ? "#3d2028" : "transparent"
                                        border.color: removeSubHover.hovered ? "#f38ba8" : "#313244"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            font.pixelSize: 15
                                            font.bold: true
                                            color: removeSubHover.hovered ? "#f38ba8" : "#6c7086"
                                        }
                                        HoverHandler { id: removeSubHover }
                                        TapHandler { onTapped: CategoryController.removeSubcategory(catRow.index, subRow.index) }
                                    }
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
            nameField.text = name
            selectedColor = color
            open()
        }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: categoryDialog.editIndex < 0 ? "New category" : "Edit category"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }

            RowLayout {
                spacing: 10
                Text { Layout.preferredWidth: 50; text: "Name"; font.pixelSize: 15; color: "#6c7086" }
                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    placeholderText: "e.g. Health"
                    placeholderTextColor: "#45475a"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    background: Rectangle {
                        color: "#11111b"
                        radius: 4
                        border.color: nameField.activeFocus ? "#89b4fa" : "#313244"
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
                            var trimmed = nameField.text.trim()
                            if (trimmed.length === 0)
                                return
                            if (categoryDialog.editIndex < 0) {
                                CategoryController.addCategory(trimmed, categoryDialog.selectedColor)
                            } else {
                                CategoryController.renameCategory(categoryDialog.editIndex, trimmed)
                                CategoryController.setCategoryColor(categoryDialog.editIndex, categoryDialog.selectedColor)
                            }
                            categoryDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ── Add / edit subcategory dialog ───────────────────────────────────────
    Popup {
        id: subcategoryDialog
        anchors.centerIn: parent
        width: 380
        padding: 20
        modal: true

        property int parentCategoryIndex: -1
        property int editSubIndex: -1
        property string selectedColor: "#89b4fa"

        function openFor(catIndex, subIndex, name, color) {
            parentCategoryIndex = catIndex
            editSubIndex = subIndex
            subNameField.text = name
            selectedColor = color
            open()
        }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: subcategoryDialog.editSubIndex < 0 ? "New subcategory" : "Edit subcategory"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }

            RowLayout {
                spacing: 10
                Text { Layout.preferredWidth: 50; text: "Name"; font.pixelSize: 15; color: "#6c7086" }
                TextField {
                    id: subNameField
                    Layout.fillWidth: true
                    placeholderText: "e.g. Pharmacy"
                    placeholderTextColor: "#45475a"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    background: Rectangle {
                        color: "#11111b"
                        radius: 4
                        border.color: subNameField.activeFocus ? "#89b4fa" : "#313244"
                    }
                }
            }

            Text { text: "Color"; font.pixelSize: 15; color: "#6c7086" }
            RowLayout {
                spacing: 10
                Rectangle {
                    width: 32; height: 32; radius: 16
                    color: subcategoryDialog.selectedColor
                    border.width: 2
                    border.color: "#45475a"
                    TapHandler {
                        onTapped: {
                            subcategoryColorDialog.selectedColor = subcategoryDialog.selectedColor
                            subcategoryColorDialog.open()
                        }
                    }
                }
                Rectangle {
                    width: 110; height: 26; radius: 4
                    color: subColorHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Choose color…"; font.pixelSize: 14; color: "#cdd6f4" }
                    HoverHandler { id: subColorHover }
                    TapHandler {
                        onTapped: {
                            subcategoryColorDialog.selectedColor = subcategoryDialog.selectedColor
                            subcategoryColorDialog.open()
                        }
                    }
                }
                Text { text: subcategoryDialog.selectedColor; font.pixelSize: 14; color: "#6c7086" }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: subCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: subCancelHover }
                    TapHandler { onTapped: subcategoryDialog.close() }
                }
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: subSaveHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text {
                        anchors.centerIn: parent
                        text: subcategoryDialog.editSubIndex < 0 ? "Add" : "Save"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#cdd6f4"
                    }
                    HoverHandler { id: subSaveHover }
                    TapHandler {
                        onTapped: {
                            var trimmed = subNameField.text.trim()
                            if (trimmed.length === 0 || subcategoryDialog.parentCategoryIndex < 0)
                                return
                            if (subcategoryDialog.editSubIndex < 0) {
                                CategoryController.addSubcategory(subcategoryDialog.parentCategoryIndex, trimmed, subcategoryDialog.selectedColor)
                            } else {
                                CategoryController.renameSubcategory(subcategoryDialog.parentCategoryIndex, subcategoryDialog.editSubIndex, trimmed)
                                CategoryController.setSubcategoryColor(subcategoryDialog.parentCategoryIndex, subcategoryDialog.editSubIndex, subcategoryDialog.selectedColor)
                            }
                            subcategoryDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ── Native color pickers ────────────────────────────────────────────────
    // QtQuick.Dialogs.ColorDialog (verified against doc.qt.io/qt-6/qml-qtquick-
    // dialogs-colordialog.html — properties: selectedColor, options; inherited
    // open()/accepted() from Dialog). One per edit dialog since categoryDialog
    // and subcategoryDialog each track their own in-progress selectedColor.
    ColorDialog {
        id: categoryColorDialog
        title: "Choose category color"
        onAccepted: categoryDialog.selectedColor = page.colorToHex(selectedColor)
    }

    ColorDialog {
        id: subcategoryColorDialog
        title: "Choose subcategory color"
        onAccepted: subcategoryDialog.selectedColor = page.colorToHex(selectedColor)
    }
}