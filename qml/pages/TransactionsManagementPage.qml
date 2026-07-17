import QtQuick
import QtQuick.Layouts

// ── Transactions Management — sidebar landing page ────────────────────────────
// Renamed from "Data Entry" (see ARCHITECTURE.MD § Transactions Management).
// All real functionality now lives in the two sidebar actions under this
// category — "Import Transactions" (ImportSnapshotPage.qml, see §
// Statement Import) and "Manage Categories" (CategoriesManagePage.qml, see §
// Category Management) — so this landing page is a minimal placeholder
// rather than mock content. The previous mock snapshot form / transaction
// entry / history / annual-summary sections (and the now-dead
// DataEntryController that backed them) were removed entirely per product
// decision: they never persisted anything and duplicated what the two real
// actions already do.

Item {
    id: page

    ColumnLayout {
        x: 20; y: 20
        width: parent.width - 40
        spacing: 12

        PageHeader { title: "Transactions Management" }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Use the sidebar actions to work with transactions: \"Import Transactions\" to bring in "
                + "a bank statement file, or \"Manage Categories\" to edit the category/subcategory list "
                + "used to classify them. Expand this category in the sidebar (the arrow) to see both."
            font.pixelSize: 15
            color: "#6c7086"
        }
    }
}
