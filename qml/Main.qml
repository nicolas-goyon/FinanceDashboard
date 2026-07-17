pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── App shell ─────────────────────────────────────────────────────────────────
// Replaces the Qt Widgets MainWindow. Catppuccin Mocha palette:
//   Base #11111b · Mantle #181825 · Surface0 #1e1e2e · Surface1 #313244
//   Overlay0 #6c7086 · Text #cdd6f4 · Blue #89b4fa
//
// Layout: left sidebar (collapsible category list) instead of a top TabBar.
// Each of the 8 categories mirrors a PROJECT.md page or a Transactions/
// Account/Database Management split page 1:1 (see ARCHITECTURE.MD § App
// Status). Clicking a category label selects its page; clicking the small
// arrow expands/collapses its action list without changing the page.
//
// ── Navigation model — single source of truth ───────────────────────────────
// `root.currentViewId` is the ONLY piece of state that says "what's on
// screen". Every category and every wired action has a matching id in the
// `categories` array below; both the StackLayout's current page and every
// overlay's active flag are derived from this one property, and so is the
// sidebar highlight (SidebarCategory.current / currentActionId, computed by
// `isCategoryActive()` below). Earlier this app tracked navigation as
// `currentIndex` (StackLayout position) PLUS five independent
// `showXxxOverlay` booleans that every entry point had to clear by hand —
// clicking a sidebar *action* (e.g. "Manage Accounts") only ever flipped an
// overlay flag and never touched `currentIndex`, so the sidebar kept
// highlighting whatever category was selected *before*, not the one whose
// action you were actually looking at. Collapsing all of that into one
// `currentViewId` string removes the class of bug entirely: there is now
// exactly one thing to set from any navigation entry point, and the sidebar
// reads the same value used to pick the visible page. See ARCHITECTURE.MD §
// Sidebar Navigation.
ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 780
    title: "Finance Dashboard"
    color: "#11111b"

    property string currentViewId: "overview"

    // Which account (index into AccountController.accounts) the
    // AccountTransactionsPage overlay should show — set right before
    // currentViewId becomes "accountTransactions" (see the accountsPage
    // Loader's onViewTransactionsRequested below).
    property int accountTransactionsIndex: -1

    // Overlay views: full-content pages reached from a sidebar action (or,
    // for "accountTransactions", from a button inside another overlay) that
    // replace the StackLayout rather than living in it — see the Loaders
    // below and ARCHITECTURE.MD § Sidebar Navigation → Overlay pages.
    readonly property var overlayViewIds: [
        "importTransactions", "manageCategories",
        "manageAccounts", "accountTypes", "accountTransactions"
    ]
    readonly property bool isOverlayView: root.overlayViewIds.indexOf(root.currentViewId) !== -1

    // One entry per StackLayout page, in the same order as its children
    // below — `stackIndex` looks a category up positionally, same
    // "array-order = child-order" contract the previous currentIndex model
    // used, just keyed by id instead of a bare int so callers never juggle
    // magic numbers.
    readonly property var categories: [
        {
            id: "overview", label: "Overview",
            actions: [
                { id: "netWorthTrend", label: "Net Worth Trend" },
                { id: "allocationBreakdown", label: "Allocation Breakdown" },
                { id: "monthlyNetChange", label: "Monthly Net Change" },
                { id: "kpiSummary", label: "KPI Summary" }
            ]
        },
        {
            id: "cashflow", label: "Cash Flow",
            actions: [
                { id: "incomeVsExpenses", label: "Income vs Expenses" },
                { id: "spendingByCategory", label: "Spending by Category" },
                { id: "categoryTrends", label: "Category Trends" },
                { id: "biggestExpenses", label: "Biggest Expenses" }
            ]
        },
        {
            id: "savings", label: "Savings",
            actions: [
                { id: "balanceHistory", label: "Balance History" },
                { id: "livretCeilingsGoals", label: "Livret Ceilings & Goals" },
                { id: "interestEarned", label: "Interest Earned" },
                { id: "blendedYield", label: "Blended Yield" }
            ]
        },
        {
            id: "investments", label: "Investments",
            actions: [
                { id: "portfolioVsDeposits", label: "Portfolio vs Deposits (P&L)" },
                { id: "assetAllocation", label: "Asset Allocation" },
                { id: "monthlyPerformance", label: "Monthly Performance" }
            ]
        },
        {
            id: "projects", label: "Projects",
            actions: [
                { id: "projectOverview", label: "Project Overview" },
                { id: "loanSimulator", label: "Loan Simulator" },
                { id: "saveVsBorrowComparison", label: "Save vs Borrow Comparison" }
            ]
        },
        {
            id: "transactionsManagement", label: "Transactions Management",
            actions: [
                { id: "importTransactions", label: "Import Transactions" },
                { id: "manageCategories", label: "Manage Categories" }
            ]
        },
        {
            id: "accountManagement", label: "Account Management",
            actions: [
                { id: "manageAccounts", label: "Manage Accounts" },
                { id: "accountTypes", label: "Account types & categories" }
            ],
            // Reached only via the "Transactions" button inside Manage
            // Accounts, not a sidebar action of its own — still belongs to
            // this category for sidebar-highlight purposes.
            relatedViewIds: ["accountTransactions"]
        },
        {
            id: "databaseManagement", label: "Database Management",
            actions: []
        }
    ]

    // True if `currentViewId` is this category's own page, one of its
    // actions, or one of its relatedViewIds — i.e. "the user is currently
    // anywhere inside this category". Single place that knows how to answer
    // that question; both the header highlight and the auto-expand behavior
    // in SidebarCategory read it via the `current` prop below.
    function isCategoryActive(category) {
        if (category.id === root.currentViewId)
            return true
        for (var i = 0; i < category.actions.length; ++i)
            if (category.actions[i].id === root.currentViewId)
                return true
        return category.relatedViewIds ? category.relatedViewIds.indexOf(root.currentViewId) !== -1 : false
    }

    // Position of the category whose id === currentViewId, for
    // StackLayout.currentIndex. Falls back to 0 (Overview) when currentViewId
    // is an overlay id — harmless since the StackLayout is hidden behind the
    // overlay in that case anyway (see `isOverlayView` above).
    readonly property int stackIndex: {
        for (var i = 0; i < root.categories.length; ++i)
            if (root.categories[i].id === root.currentViewId)
                return i
        return 0
    }

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
                        Layout.fillWidth: true
                        categoryLabel: modelData.label
                        actions: modelData.actions
                        current: root.isCategoryActive(modelData)
                        currentActionId: root.currentViewId
                        onSelected: root.currentViewId = modelData.id
                        onActionTriggered: (actionId) => {
                            // Only actions with a real page behind them
                            // change the view; every other action on the
                            // first 5 categories is still a no-op
                            // placeholder (see ARCHITECTURE.MD § Sidebar
                            // Navigation). Add a page + its id here (and to
                            // `categories` above) to wire up a new one.
                            if (root.overlayViewIds.indexOf(actionId) !== -1
                                    && actionId !== "accountTransactions") {
                                root.currentViewId = actionId
                            }
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
        visible: !root.isOverlayView
        currentIndex: root.stackIndex

        OverviewPage                  {}
        CashFlowPage                  {}
        SavingsPage                   {}
        InvestmentsPage               {}
        ProjectsPage                  {}
        TransactionsManagementPage    {}
        AccountManagementLandingPage  {}
        DatabaseManagementPage        {}
    }

    // ── Overlay — Import Transactions (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ────────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.currentViewId === "importTransactions"
        visible: active
        sourceComponent: importPageComponent
    }

    Component {
        id: importPageComponent
        ImportSnapshotPage {
            onCloseRequested: root.currentViewId = "transactionsManagement"
        }
    }

    // ── Overlay — Manage Categories (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ──────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.currentViewId === "manageCategories"
        visible: active
        sourceComponent: categoriesPageComponent
    }

    Component {
        id: categoriesPageComponent
        CategoriesManagePage {
            onCloseRequested: root.currentViewId = "transactionsManagement"
        }
    }

    // ── Overlay — Manage Accounts (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ──────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.currentViewId === "manageAccounts"
        visible: active
        sourceComponent: accountsPageComponent
    }

    Component {
        id: accountsPageComponent
        AccountsManagePage {
            onCloseRequested: root.currentViewId = "accountManagement"
            // "Transactions" on an account row routes to a full page, not a
            // modal (product decision — see ARCHITECTURE.MD § Account
            // Management) — remember which account, then switch views.
            onViewTransactionsRequested: (accountIndex) => {
                root.accountTransactionsIndex = accountIndex
                root.currentViewId = "accountTransactions"
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
        active: root.currentViewId === "accountTransactions"
        visible: active
        sourceComponent: accountTransactionsPageComponent
    }

    Component {
        id: accountTransactionsPageComponent
        AccountTransactionsPage {
            accountIndex: root.accountTransactionsIndex
            // Back arrow returns to Manage Accounts, not the sidebar view —
            // this page is reached *from* that page, not from the sidebar.
            onCloseRequested: root.currentViewId = "manageAccounts"
        }
    }

    // ── Overlay — Account Types & Categories (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ──────────
    Loader {
        anchors {
            top: parent.top; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.currentViewId === "accountTypes"
        visible: active
        sourceComponent: accountTypesPageComponent
    }

    Component {
        id: accountTypesPageComponent
        AccountTypesCategoriesPage {
            onCloseRequested: root.currentViewId = "accountManagement"
        }
    }
}
