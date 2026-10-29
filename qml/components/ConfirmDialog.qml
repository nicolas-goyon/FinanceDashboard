import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── ConfirmDialog ────────────────────────────────────────────────────────────
// Shared shell for every destructive-action confirmation (delete category,
// delete account, reset database, reset to mock data). `confirmVariant`
// defaults to "danger" (red) but Database Management's "Reset to Mock Data"
// confirm step intentionally reuses this with variant "primary" (blue),
// matching that action's own trigger-button color, so it stays available as
// a per-call override rather than being hard-coded red — see the
// component-duplication survey in ARCHITECTURE.MD § Shared Components.
Popup {
    id: root
    anchors.centerIn: parent
    width: 340
    padding: 20
    modal: true

    property string dialogTitle: ""
    property string message: ""
    property string confirmText: "Delete"
    property string confirmVariant: "danger" // "danger" | "primary"

    signal confirmed()

    background: Rectangle { color: Theme.mantle; border.color: Theme.surface1; radius: 8 }

    contentItem: ColumnLayout {
        spacing: 14

        Text {
            text: root.dialogTitle
            font.pixelSize: 19
            font.bold: true
            color: Theme.text
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: root.message
            font.pixelSize: 14
            color: Theme.subtext0
        }
        RowLayout {
            Layout.topMargin: 6
            Item { Layout.fillWidth: true }
            PillButton { text: "Cancel"; variant: "neutral"; onClicked: root.close() }
            PillButton {
                text: root.confirmText
                variant: root.confirmVariant
                onClicked: { root.confirmed(); root.close() }
            }
        }
    }
}
