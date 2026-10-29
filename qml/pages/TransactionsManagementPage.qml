import QtQuick
import QtQuick.Layouts

// ── Transactions — sidebar landing page ───────────────────────────────────────
// Category renamed from "Transactions Management" to "Transactions"
// (2026-07-18, see ARCHITECTURE.MD § Transactions), and before that from
// "Data Entry". All real functionality lives in this category's three
// sidebar actions — "Import Transactions" (ImportSnapshotPage.qml, see §
// Statement Import), "Categories & Subcategories" (CategoriesManagePage.qml,
// see § Category Management, renamed from "Manage Categories" in the same
// pass), and "View & Edit Transactions" (AccountTransactionsPage.qml, moved
// here from Account Management in the same pass — see § Account Management)
// — so this landing page stays a minimal placeholder rather than mock
// content. Backing QML file/type name kept as TransactionsManagementPage —
// only the label changed, same "label ≠ file/type name" precedent as
// SnapshotImportController.

Item {
    id: page

    ColumnLayout {
        x: 20; y: 20
        width: parent.width - 40
        spacing: 12

        PageHeader { title: "Transactions" }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Use the sidebar actions to work with transactions: \"Import Transactions\" to bring in "
                + "a bank statement file, \"Categories & Subcategories\" to edit the category list used to "
                + "classify them, or \"View & Edit Transactions\" to see and fix the saved transaction "
                + "table for any account. Expand this category in the sidebar (the arrow) to see all three."
            font.pixelSize: 15
            color: Theme.overlay0
        }
    }
}
