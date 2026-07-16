import QtQuick

// ── EmptyState ───────────────────────────────────────────────────────────────
// "No … yet" placeholder text shown under a list's header when its model is
// empty (Categories, Accounts, Account Types & Categories, Account
// Transactions, the import review table). Style was already 100% consistent
// across every instance — this just removes the copy-paste.
Text {
    property string message: "Nothing here yet."
    text: message
    font.pixelSize: 15
    color: "#6c7086"
}
