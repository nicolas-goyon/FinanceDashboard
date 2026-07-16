import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── PageHeader ───────────────────────────────────────────────────────────────
// Shared page-header row: optional back arrow, title, optional "[MOCK]"
// marker, a stretch, then any caller-supplied trailing content (add
// buttons, status badges, filters — ordinary children, laid out after the
// stretch in declaration order via the default property, same slot
// convention as ChartCard.content). `showBack: true` on pages reached by
// drilling into a sidebar action (Categories/Accounts/AccountTypes/Import/
// AccountTransactions); left false on top-level category pages and the 5
// mock analytics pages. See ARCHITECTURE.MD § Shared Components.
RowLayout {
    id: root
    Layout.fillWidth: true
    spacing: 12

    property bool showBack: false
    property string title: ""
    property bool mock: false
    default property alias content: trailingRow.data

    signal backClicked()

    Rectangle {
        visible: root.showBack
        width: 26; height: 26; radius: 4
        color: backHover.hovered ? "#181825" : "transparent"
        Text {
            anchors.centerIn: parent
            text: "←"
            font.pixelSize: 19
            color: "#89b4fa"
        }
        HoverHandler { id: backHover }
        TapHandler { onTapped: root.backClicked() }
    }

    Text {
        text: root.title
        font.pixelSize: 23
        font.bold: true
        color: "#cdd6f4"
    }

    Text {
        visible: root.mock
        text: "[MOCK]"
        font.pixelSize: 14
        color: "#f38ba8"
    }

    Item { Layout.fillWidth: true }

    RowLayout {
        id: trailingRow
        spacing: 12
    }
}
