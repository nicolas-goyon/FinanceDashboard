import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── NewOptionDialog ──────────────────────────────────────────────────────────
// The small dialog of a "＋ New…" row of a list (#237, AddOptionComboBox,
// ColorComboBox): a name, and a colour when `withColor` is true. Save emits
// `optionAccepted(name, color)` with the trimmed name, then closes. The caller
// adds the option with its controller and selects it.
//
// The parent is the overlay of the window: the dialog is at the centre of the
// window, also when it opens from another dialog (the account form, the edit
// dialog of a transaction).
FormDialog {
    id: root
    parent: Overlay.overlay
    saveText: qsTr("Add")

    property bool withColor: false

    signal optionAccepted(string name, string color)

    // `title`: the title of the dialog. `placeholder`: an example name.
    // `color`: the colour to start from ("#rrggbb"), when `withColor`.
    function openWith(title, placeholder, color) {
        dialogTitle = title
        nameRow.placeholderText = placeholder
        nameRow.text = ""
        if (color !== undefined)
            colorRow.selectedColor = color
        open()
        nameRow.field.forceActiveFocus()
    }

    FormRow { id: nameRow; label: qsTr("Name") }

    ColumnLayout {
        visible: root.withColor
        spacing: 8
        Text { text: qsTr("Color"); font.pixelSize: 15; color: Theme.overlay0 }
        ColorPickerRow { id: colorRow; dialogTitle: qsTr("Choose color") }
    }

    onSaveClicked: {
        var trimmed = nameRow.text.trim()
        if (trimmed.length === 0)
            return
        close()
        root.optionAccepted(trimmed, root.withColor ? colorRow.selectedColor : "")
    }
}
