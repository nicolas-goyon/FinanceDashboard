import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── Account Management — sidebar landing page ─────────────────────────────────
// Own sidebar category, distinct from AccountsManagePage.qml (the "Manage
// Accounts" action's overlay — note the different name: this is the
// category's landing page, that is the accounts CRUD page). All real
// functionality lives in this category's two sidebar actions — "Manage
// Accounts" (AccountsManagePage.qml) and "Account types & categories"
// (AccountTypesCategoriesPage.qml) — so this landing page is a minimal
// placeholder, same convention as TransactionsManagementPage.qml (see
// ARCHITECTURE.MD § Account Management).

Item {
    id: page

    ColumnLayout {
        x: 20; y: 20
        width: parent.width - 40
        spacing: 12

        PageHeader { title: "Account Management" }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Use the sidebar actions to manage accounts: \"Manage Accounts\" to add/edit/remove "
                + "accounts, link hash fingerprints, and view an account's transactions, or \"Account "
                + "types & categories\" to edit the type/category lists accounts are classified by. "
                + "Expand this category in the sidebar (the arrow) to see both."
            font.pixelSize: 15
            color: "#6c7086"
        }
    }
}
