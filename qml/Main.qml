import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── App shell ─────────────────────────────────────────────────────────────────
// Replaces the Qt Widgets MainWindow. Catppuccin Mocha palette:
//   Base #11111b · Mantle #181825 · Surface0 #1e1e2e · Surface1 #313244
//   Overlay0 #6c7086 · Text #cdd6f4 · Blue #89b4fa
//
// Layout: left sidebar (collapsible category list) instead of a top TabBar.
// The first 5 categories mirror the first 5 PROJECT.md pages 1:1. The
// former 6th ("Data Entry") was restructured into three lighter categories —
// Transactions Management, Account Management, Database Management — plus
// the non-spec "Other - To remove" (see ARCHITECTURE.MD § Transactions
// Management / § Account Management / § Database Management for the
// rationale). Clicking a category label selects its page; clicking the
// small arrow expands/collapses its pre-filled action list.

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 780
    title: "Finance Dashboard"
    color: "#11111b"

    property int currentIndex: 0

    // Five sidebar actions are wired up so far, each opening a full-content
    // overlay (via the Loaders below): "Import Transactions" and "Manage
    // Categories" under Transactions Management; "Manage Accounts" and
    // "Account types & categories" under Account Management; plus the
    // account-scoped Transactions page reached *from* Manage Accounts
    // (not a sidebar action itself). Database Management has no actions —
    // its landing page holds the real controls directly.
    property bool showImportOverlay: false
    property bool showCategoriesOverlay: false
    property bool showAccountsOverlay: false
    property bool showAccountTypesOverlay: false
    property bool showAccountTransactionsOverlay: false

    // Which account (index into AccountController.accounts) the
    // AccountTransactionsPage overlay should show — set right before
    // showAccountTransactionsOverlay flips true (see the accountsPageComponent
    // Loader's onViewTransactionsRequested below).
    property int accountTransactionsIndex: -1

    readonly property var categories: [
        {
            label: "Overview",
            actions: ["Net Worth Trend", "Allocation Breakdown", "Monthly Net Change", "KPI Summary"]
        },
        {
            label: "Cash Flow",
            actions: ["Income vs Expenses", "Spending by Category", "Category Trends", "Biggest Expenses"]
        },
        {
            label: "Savings",
            actions: ["Balance History", "Livret Ceilings & Goals", "Interest Earned", "Blended Yield"]
        },
        {
            label: "Investments",
            actions: ["Portfolio vs Deposits (P&L)", "Asset Allocation", "Monthly Performance"]
        },
        {
            label: "Projects",
            actions: ["Project Overview", "Loan Simulator", "Save vs Borrow Comparison"]
        },
        {
            label: "Transactions Management",
            actions: ["Import Transactions", "Manage Categories"]
        },
        {
            label: "Account Management",
            actions: ["Manage Accounts", "Account types & categories"]
        },
        {
            label: "Database Management",
            actions: []
        },
        {
            label: "Other - To remove",
            actions: ["AnalyticsTab.qml", "DatabaseTab.qml", "EntryTab.qml", "ImportTab.qml",
                      "OverviewTab.qml", "PortfolioTab.qml", "PlaceholderTab.qml"]
        }
    ]

    // ── Sidebar ───────────────────────────────────────────────────────────────
    Rectangle {
        id: sidebar
        anchors { top: parent.top; bottom: parent.bottom; left: parent.left }
        width: 240
        color: "#181825"

        Flickable {
            anchors.fill: parent
            contentHeight: sidebarColumn.implicitHeight
            clip: true

            ColumnLayout {
                id: sidebarColumn
                width: sidebar.width
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    Layout.margins: 16
                    Layout.bottomMargin: 4
                    text: "FINANCE DASHBOARD"
                    color: "#6c7086"
                    font { pixelSize: 14; bold: true }
                }

                Repeater {
                    model: root.categories
                    delegate: SidebarCategory {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        categoryLabel: modelData.label
                        actions: modelData.actions
                        current: index === root.currentIndex
                        onSelected: {
                            // Clicking a category must return to the sidebar-driven
                            // StackLayout view even if an overlay page is currently
                            // open — otherwise the Loader stays on top,
                            // StackLayout.visible stays false, and currentIndex
                            // changes invisibly behind it until the overlay's own
                            // closeRequested() (back arrow) fires. See
                            // ARCHITECTURE.MD § Sidebar Navigation. Every new
                            // overlay flag must be added here too.
                            root.showImportOverlay = false
                            root.showCategoriesOverlay = false
                            root.showAccountsOverlay = false
                            root.showAccountTypesOverlay = false
                            root.showAccountTransactionsOverlay = false
                            root.currentIndex = index
                        }
                        onActionTriggered: (action) => {
                            if (modelData.label === "Transactions Management" && action === "Import Transactions") {
                                root.showCategoriesOverlay = false
                                root.showAccountsOverlay = false
                                root.showAccountTypesOverlay = false
                                root.showAccountTransactionsOverlay = false
                                root.showImportOverlay = true
                            } else if (modelData.label === "Transactions Management" && action === "Manage Categories") {
                                root.showImportOverlay = false
                                root.showAccountsOverlay = false
                                root.showAccountTypesOverlay = false
                                root.showAccountTransactionsOverlay = false
                                root.showCategoriesOverlay = true
                            } else if (modelData.label === "Account Management" && action === "Manage Accounts") {
                                root.showImportOverlay = false
                                root.showCategoriesOverlay = false
                                root.showAccountTypesOverlay = false
                                root.showAccountTransactionsOverlay = false
                                root.showAccountsOverlay = true
                            } else if (modelData.label === "Account Management" && action === "Account types & categories") {
                                root.showImportOverlay = false
                                root.showCategoriesOverlay = false
                                root.showAccountsOverlay = false
                                root.showAccountTransactionsOverlay = false
                                root.showAccountTypesOverlay = true
                            }
                            // All other actions are still placeholders (no-op).
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: sidebarDivider
        anchors { top: parent.top; bottom: parent.bottom; left: sidebar.right }
        width: 1; color: "#313244"
    }

    // ── Page content ─────────────────────────────────────────────────────────
    StackLayout {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        visible: !root.showImportOverlay && !root.showCategoriesOverlay && !root.showAccountsOverlay
            && !root.showAccountTypesOverlay && !root.showAccountTransactionsOverlay
        currentIndex: root.currentIndex

        OverviewPage                  {}
        CashFlowPage                  {}
        SavingsPage                   {}
        InvestmentsPage               {}
        ProjectsPage                  {}
        TransactionsManagementPage    {}
        AccountManagementLandingPage  {}
        DatabaseManagementPage        {}
        OtherLegacyPage                {}
    }

    // ── Overlay — Import Transactions (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ────────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.showImportOverlay
        visible: active
        sourceComponent: importPageComponent
    }

    Component {
        id: importPageComponent
        ImportSnapshotPage {
            onCloseRequested: root.showImportOverlay = false
        }
    }

    // ── Overlay — Manage Categories (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ──────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.showCategoriesOverlay
        visible: active
        sourceComponent: categoriesPageComponent
    }

    Component {
        id: categoriesPageComponent
        CategoriesManagePage {
            onCloseRequested: root.showCategoriesOverlay = false
        }
    }

    // ── Overlay — Manage Accounts (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ──────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.showAccountsOverlay
        visible: active
        sourceComponent: accountsPageComponent
    }

    Component {
        id: accountsPageComponent
        AccountsManagePage {
            onCloseRequested: root.showAccountsOverlay = false
            // "Transactions" on an account row routes to a full page, not a
            // modal (product decision — see ARCHITECTURE.MD § Account
            // Management) — swap overlays and remember which account.
            onViewTransactionsRequested: (accountIndex) => {
                root.accountTransactionsIndex = accountIndex
                root.showAccountsOverlay = false
                root.showAccountTransactionsOverlay = true
            }
        }
    }

    // ── Overlay — Account Transactions (reached from Manage Accounts, not a
    //    sidebar action — see above). Lazily loaded/unloaded via
    //    Loader.active. ─────────────────────────────────────────────────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.showAccountTransactionsOverlay
        visible: active
        sourceComponent: accountTransactionsPageComponent
    }

    Component {
        id: accountTransactionsPageComponent
        AccountTransactionsPage {
            accountIndex: root.accountTransactionsIndex
            // Back arrow returns to Manage Accounts, not the sidebar view —
            // this page is reached *from* that page, not from the sidebar.
            onCloseRequested: {
                root.showAccountTransactionsOverlay = false
                root.showAccountsOverlay = true
            }
        }
    }

    // ── Overlay — Account Types & Categories (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ──────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.showAccountTypesOverlay
        visible: active
        sourceComponent: accountTypesPageComponent
    }

    Component {
        id: accountTypesPageComponent
        AccountTypesCategoriesPage {
            onCloseRequested: root.showAccountTypesOverlay = false
        }
    }
}
