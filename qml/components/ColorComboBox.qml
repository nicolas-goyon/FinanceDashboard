pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── ColorComboBox ──────────────────────────────────────────────────────────
// ComboBox variant that shows a color swatch next to each item's name, both
// in the dropdown list (delegate) and in the collapsed field itself
// (contentItem) — see ARCHITECTURE.MD § Statement Import for why: the
// transaction review table needed the category/subcategory colors visible
// while picking, not just once a row is saved.
//
// Items are plain {name, color, ...} objects — CategoryController.categories
// and each category's subcategories are already this shape, so this is used
// as-is for both the category and subcategory selects in
// ImportSnapshotPage.qml. Follows the delegate customization pattern from
// the Qt Quick Controls docs ("Customizing ComboBox"): the delegate declares
// `required property var model` and reads named roles off it
// (`delegate.model.name` / `.color`), matching the `pragma ComponentBehavior:
// Bound` example there.
//
// currentIndex is driven from `currentName` (a plain string prop, since
// callers only have the row's category/subcategory name, not an index) via
// a declarative binding — same "breaks after user picks an item" caveat as
// every other ComboBox in this app (see ImportSnapshotPage.qml's original
// `currentIndex: model.indexOf(modelData.category)`), which is fine here
// because the Repeater it lives in rebuilds its delegates whenever
// `controller.transactionRows()` changes.

ComboBox {
    id: control

    // Array of {name, color, ...} objects.
    property var items: []
    // Name of the currently selected item; drives currentIndex.
    property string currentName: ""
    // Swatch color used when nothing is selected / name not found.
    property color emptyColor: "#45475a"

    signal nameActivated(string name)

    model: items
    textRole: "name"

    function indexForName(name) {
        for (var i = 0; i < items.length; ++i)
            if (items[i].name === name)
                return i
        return -1
    }

    currentIndex: indexForName(currentName)
    onActivated: (index) => control.nameActivated(control.textAt(index))

    delegate: ItemDelegate {
        id: delegate

        required property var model
        required property int index

        width: control.width
        highlighted: control.highlightedIndex === index

        contentItem: RowLayout {
            spacing: 8
            Rectangle {
                width: 12; height: 12; radius: 6
                color: delegate.model.color ? delegate.model.color : control.emptyColor
                border.color: "#313244"
            }
            Text {
                Layout.fillWidth: true
                text: delegate.model.name
                color: "#cdd6f4"
                font.pixelSize: 15
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    contentItem: RowLayout {
        spacing: 6

        Rectangle {
            width: 10; height: 10; radius: 5
            visible: control.currentIndex >= 0
            color: control.currentIndex >= 0 ? control.items[control.currentIndex].color : "transparent"
            border.color: "#313244"
        }
        Text {
            Layout.fillWidth: true
            // Same right-padding reservation as the default ComboBox
            // contentItem (Qt Quick Controls "Customizing ComboBox" example)
            // so the text never overlaps the drop-down indicator.
            rightPadding: control.indicator ? control.indicator.width + control.spacing : 0
            text: control.displayText
            color: "#cdd6f4"
            font.pixelSize: 15
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }
}
