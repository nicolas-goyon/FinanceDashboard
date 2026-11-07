import QtQuick
import QtQuick.Controls

// ── AddOptionComboBox ────────────────────────────────────────────────────────
// A ComboBox with a last row "＋ New…" in its list (#237). A click on that row
// emits `addRequested()`: the caller opens a dialog (NewOptionDialog, or the
// account form), adds the option, then selects it. The row is not an item of
// the model (see AddOptionPopup.qml).
//
// Use it for a list of things that are simple to make: a name, sometimes a
// colour. Keep a plain ComboBox for a fixed list (currency, language, update
// channel, import template, month) and for a list that selects an existing
// item to show or change (the account selectors of the pages).
ComboBox {
    id: control

    // Text of the last row, for example qsTr("＋ New type…").
    property string addText: ""

    signal addRequested()

    popup: AddOptionPopup {
        comboBox: control
        addText: control.addText
        onAddClicked: control.addRequested()
    }
}
