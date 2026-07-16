pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// ── ColorComboBox ──────────────────────────────────────────────────────────
// ComboBox variant that shows each item's color as the *background* of the
// row/field itself (rather than a small swatch badge next to the name) — see
// ARCHITECTURE.MD § Statement Import for why: the transaction review table
// needed the category/subcategory colors visible while picking, not just
// once a row is saved.
//
// Items are plain {name, color, ...} objects — CategoryController.categories
// and each category's subcategories are already this shape, so this is used
// as-is for both the category and subcategory selects in
// ImportSnapshotPage.qml.
//
// contentItem/delegate follow the "Customizing ComboBox" pattern from the
// Qt Quick Controls docs (doc.qt.io/qt-6/qtquickcontrols-customize.html
// #customizing-combobox): plain Text/ItemDelegate, with the delegate's
// `required property var model`/`index` read off named roles (matching the
// `pragma ComponentBehavior: Bound` example there). Unlike that doc example,
// `indicator` is left at its Basic-style default (qtdeclarative
// src/quickcontrols/basic/ComboBox.qml), which already sets
// control.rightPadding to `padding + indicator.width + spacing` — so the
// indicator's space is already excluded from this contentItem's box before
// it's even created. The doc example additionally reserves
// `control.indicator.width + control.spacing` *inside* its own custom
// contentItem, which only makes sense there because it also supplies its
// own custom `indicator:`; copying that expression here double-reserved the
// same space and left almost nothing for the category/subcategory names in
// ImportSnapshotPage.qml's 115px columns, cropping them to 2-3 characters.
// A small fixed inset (8px) is the correct amount here.
//
// The previous version (before backgrounds carried the color) also wrapped
// the content in a RowLayout with a circular swatch Rectangle ahead of the
// text, which ate further into that same width; that's gone too now that
// the background carries the color instead.
//
// Text color is chosen per item from the background color's perceived
// luminance (readableTextColor) so it stays legible against arbitrary
// category colors, both light and dark.
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
    // Swatch/background color used when nothing is selected / name not found.
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

    // Perceived luminance (ITU-R BT.601 coefficients) of a background color,
    // used to pick a readable dark-or-light text color against it.
    function readableTextColor(bg) {
        var luminance = 0.299 * bg.r + 0.587 * bg.g + 0.114 * bg.b
        return luminance > 0.6 ? "#11111b" : "#cdd6f4"
    }

    currentIndex: indexForName(currentName)
    onActivated: (index) => control.nameActivated(control.textAt(index))

    readonly property color currentColor: currentIndex >= 0 && items[currentIndex].color
        ? items[currentIndex].color
        : emptyColor
    readonly property color currentTextColor: readableTextColor(currentColor)

    delegate: ItemDelegate {
        id: delegate

        required property var model
        required property int index

        readonly property color itemColor: delegate.model.color ? delegate.model.color : control.emptyColor
        readonly property color itemTextColor: control.readableTextColor(itemColor)

        width: control.width
        highlighted: control.highlightedIndex === index

        background: Rectangle {
            color: delegate.itemColor
            opacity: delegate.highlighted ? 1.0 : 0.85
        }

        contentItem: Text {
            text: delegate.model.name
            color: delegate.itemTextColor
            font.pixelSize: 15
            leftPadding: 8
            rightPadding: 8
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    contentItem: Text {
        // No extra indicator-width reservation here: the Basic style's own
        // ComboBox.qml already sets control.rightPadding to
        // `padding + indicator.width + spacing` (see its leftPadding/
        // rightPadding bindings), which is what sizes this contentItem's
        // box in the first place — it already excludes the indicator.
        // Adding `control.indicator.width + control.spacing` again here
        // (as the Qt docs' "Customizing ComboBox" example does) double-
        // reserves that space, which is fine in that example's roomy
        // 120px box but was silently eating most of the text width in our
        // 115px category/subcategory columns, cropping names to 2-3
        // characters. A small fixed inset is all that's needed.
        leftPadding: 8
        rightPadding: 2
        text: control.displayText
        color: control.currentTextColor
        font.pixelSize: 15
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 4
        color: control.currentColor
        border.color: "#313244"
    }
}
