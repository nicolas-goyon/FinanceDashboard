import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── FormDialog ───────────────────────────────────────────────────────────────
// Shared shell for every add/edit Popup in the app (categories,
// subcategories, accounts, account types, account categories, add-hash).
// Caller supplies the title, save-button label, and form rows (FormRow /
// ColorPickerRow / any custom content) via the default property — same
// slot convention as ChartCard.content. Cancel always closes; Save emits
// `saveClicked()` so the caller can validate first and decide whether to
// actually close (matches every existing dialog's "trim, bail out on empty,
// otherwise call the controller and close" pattern).
Popup {
    id: root
    anchors.centerIn: parent
    width: 380
    padding: 20
    modal: true

    property string dialogTitle: ""
    property string saveText: "Save"
    property bool saveEnabled: true
    default property alias content: formColumn.data

    signal saveClicked()

    background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

    contentItem: ColumnLayout {
        spacing: 14

        Text {
            text: root.dialogTitle
            font.pixelSize: 19
            font.bold: true
            color: "#cdd6f4"
        }

        ColumnLayout {
            id: formColumn
            Layout.fillWidth: true
            spacing: 14
        }

        RowLayout {
            Layout.topMargin: 6
            Item { Layout.fillWidth: true }
            PillButton { text: "Cancel"; variant: "neutral"; onClicked: root.close() }
            PillButton {
                text: root.saveText
                variant: "primary"
                enabled: root.saveEnabled
                onClicked: root.saveClicked()
            }
        }
    }
}
