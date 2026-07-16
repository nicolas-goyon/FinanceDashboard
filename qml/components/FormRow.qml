import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── FormRow ──────────────────────────────────────────────────────────────────
// One "label + TextField" row, styled identically everywhere a dialog needs
// free-text input (Categories/Subcategories, Accounts, Account Types &
// Categories, the Projects new-project dialog, the add-hash dialog). The
// TextField recipe (bg #11111b, border #313244 / focus #89b4fa) was already
// 100% consistent across every dialog before this was extracted — this just
// removes the copy-paste. See ARCHITECTURE.MD § Shared Components.
RowLayout {
    id: root
    spacing: 10

    property string label: ""
    property real labelWidth: 70
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias echoMode: field.echoMode
    property alias field: field

    Text {
        Layout.preferredWidth: root.labelWidth
        text: root.label
        font.pixelSize: 15
        color: "#6c7086"
    }
    TextField {
        id: field
        Layout.fillWidth: true
        placeholderTextColor: "#45475a"
        color: "#cdd6f4"
        font.pixelSize: 15
        background: Rectangle {
            color: "#11111b"
            radius: 4
            border.color: field.activeFocus ? "#89b4fa" : "#313244"
        }
    }
}
