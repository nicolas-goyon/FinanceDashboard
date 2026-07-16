import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// ── ColorPickerRow ───────────────────────────────────────────────────────────
// Color-picker row used by every add/edit dialog with a color field
// (Categories, Subcategories, Account Categories). 32x32 circle swatch +
// "Choose color…" pill + hex text, backed by a native QtQuick.Dialogs
// ColorDialog (verified against doc.qt.io/qt-6/qml-qtquick-dialogs-
// colordialog.html — selectedColor/options properties, inherited
// open()/accepted() from Dialog).
//
// Owns its own ColorDialog instance rather than sharing one — each dialog
// needs an independent in-progress color, and a declarative binding to a
// shared ColorDialog breaks permanently the first time the user picks a
// color (see ARCHITECTURE.MD § Category Management), so a fresh instance
// per row is deliberate, not an oversight.
RowLayout {
    id: root
    spacing: 10

    property string selectedColor: "#89b4fa"
    property string dialogTitle: "Choose color"

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

    Rectangle {
        width: 32; height: 32; radius: 16
        color: root.selectedColor
        border.width: 2
        border.color: "#45475a"
        TapHandler {
            onTapped: { colorDialog.selectedColor = root.selectedColor; colorDialog.open() }
        }
    }
    Rectangle {
        width: 110; height: 26; radius: 4
        color: swatchHover.hovered ? "#313244" : "#1e1e2e"
        border.color: "#313244"
        Text { anchors.centerIn: parent; text: "Choose color…"; font.pixelSize: 14; color: "#cdd6f4" }
        HoverHandler { id: swatchHover }
        TapHandler {
            onTapped: { colorDialog.selectedColor = root.selectedColor; colorDialog.open() }
        }
    }
    Text { text: root.selectedColor; font.pixelSize: 14; color: "#6c7086" }

    ColorDialog {
        id: colorDialog
        title: root.dialogTitle
        onAccepted: root.selectedColor = root.colorToHex(selectedColor)
    }
}
