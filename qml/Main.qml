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
// Each of the 9 categories mirrors a PROJECT.md page, a Transactions/
// Account/Database Management split page or the Updates page (#117) 1:1 (see
// ARCHITECTURE.MD § App Status). Clicking a category label selects its page;
// clicking the small arrow expands/collapses its action list without changing
// the page.
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
    // "Finance Dashboard", or "Finance Dashboard (Test)" for the Test app (#130):
    // main.cpp sets it from the identity of the build (QtQuick Application
    // singleton: https://doc.qt.io/qt-6/qml-qtquick-application.html).
    title: Application.displayName
    color: "#11111b"

    property string currentViewId: "overview"

    // Which account (index into AccountController.accounts) the
    // AccountTransactionsPage overlay should show — set right before
    // currentViewId becomes "accountTransactions" (see the accountsPage
    // Loader's onViewTransactionsRequested below, and the sidebar
    // onActionTriggered handler). -1 lets the page itself fall back to the
    // first account (see AccountTransactionsPage.qml's own default), which is
    // what happens when it's reached directly from the sidebar rather than
    // via a specific account's "Transactions" button.
    property int accountTransactionsIndex: -1

    // Where the back arrow on AccountTransactionsPage returns to — it has two
    // entry points since the 2026-07-18 restructure: the sidebar's own
    // "View & Edit Transactions" action (back → "transactionsManagement",
    // its category landing page) and Manage Accounts' per-row "Transactions"
    // button (back → "manageAccounts", unchanged from before the restructure).
    property string accountTransactionsReturnViewId: "transactionsManagement"

    // Set by main.cpp when this start upgraded the database
    // (Database::upgradeReport(): versions, backup copy, duplicates removed).
    // databaseNoticePopup below shows it once.
    property string databaseNotice: ""

    // Set by main.cpp in the Test app only (#130): the database of the prod
    // app, for "Copy my prod database" on the Database Management page. Empty
    // in the prod app, which then has no such action.
    property string prodDatabasePath: ""

    // Set by main.cpp: true in the Test app (#130). UpdateController follows
    // the Experimental or Preprod channel there, and Stable in the prod app
    // (#117, #129).
    property bool isTestApp: false

    // The update check at start (#117): UpdateController waits a few seconds,
    // and does nothing when the user turned the check off.
    Component.onCompleted: {
        UpdateController.testApp = root.isTestApp
        UpdateController.startupCheck()
    }

    // Overlay views: full-content pages reached from a sidebar action (or,
    // for "accountTransactions", from a button inside another overlay) that
    // replace the StackLayout rather than living in it — see the Loaders
    // below and ARCHITECTURE.MD § Sidebar Navigation → Overlay pages.
    readonly property var overlayViewIds: [
        "importTransactions", "manageCategories",
        "manageAccounts", "accountTypes", "accountTransactions", "accountDetails"
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
                { id: "kpiSummary", label: "KPI Summary" },
                // Moved here from Account Management (2026-07-18) — this is a
                // day-to-day "look at my data" view (per-account balance
                // history/current balance), not an admin/CRUD operation like
                // its former siblings ("Manage Accounts", "Account types &
                // categories"), so it belongs with the other analysis pages
                // rather than the account-admin category. See ARCHITECTURE.MD
                // § Account History.
                { id: "accountDetails", label: "Account Details" }
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
            // Renamed from "Transactions Management" (2026-07-18) — the old
            // name implied it manages saved transactions, but its actions are
            // actually import + categorization + (now) the transaction table
            // itself. Id kept as "transactionsManagement" — only the label
            // changed, same "label ≠ id/class name" precedent as
            // SnapshotImportController backing "Import Transactions" (see
            // ARCHITECTURE.MD § Sidebar Navigation).
            id: "transactionsManagement", label: "Transactions",
            actions: [
                { id: "importTransactions", label: "Import Transactions" },
                { id: "manageCategories", label: "Categories & Subcategories" },
                // Moved here from Account Management (2026-07-18, product
                // feedback) — viewing/editing the saved transaction table is
                // a transaction-domain action, not account-admin, so it now
                // lives alongside Import Transactions / Categories &
                // Subcategories as a real sidebar action instead of being
                // reachable only via a button inside Manage Accounts. Still
                // also reachable that way (Manage Accounts → "Transactions"
                // button) — see the dual return-view handling in the
                // accountTransactions Loader/Component below and
                // ARCHITECTURE.MD § Account Management.
                { id: "accountTransactions", label: "View & Edit Transactions" }
            ]
        },
        {
            // "ADMIN" divider (rendered by the sidebar Repeater below) starts
            // here — Account Management and Database Management are
            // destructive/CRUD operations on the app's own data model, not
            // day-to-day analysis or data entry, per product feedback
            // (2026-07-18). See ARCHITECTURE.MD § Sidebar Navigation.
            id: "accountManagement", label: "Account Management",
            sectionLabel: "ADMIN",
            actions: [
                { id: "manageAccounts", label: "Manage Accounts" },
                { id: "accountTypes", label: "Account types & categories" }
            ]
        },
        {
            id: "databaseManagement", label: "Database Management",
            actions: []
        },
        {
            // The update check and the versions of the channel (#117).
            id: "updates", label: "Updates",
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
                    // Delegate is a small ColumnLayout (not a bare
                    // SidebarCategory) so it can optionally render an "ADMIN"
                    // section divider above categories that declare
                    // `sectionLabel` (currently just Account Management —
                    // see `categories` above and ARCHITECTURE.MD § Sidebar
                    // Navigation).
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 0

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 12
                            Layout.bottomMargin: 4
                            visible: modelData.sectionLabel !== undefined
                            spacing: 4

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.leftMargin: 16
                                Layout.rightMargin: 16
                                height: 1
                                color: "#313244"
                            }
                            Text {
                                Layout.leftMargin: 16
                                text: modelData.sectionLabel !== undefined ? modelData.sectionLabel : ""
                                color: "#6c7086"
                                font { pixelSize: 11; bold: true; letterSpacing: 1 }
                            }
                        }

                        SidebarCategory {
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
                                // Navigation). Add a page + its id here (and
                                // to `categories` above) to wire up a new
                                // one.
                                if (root.overlayViewIds.indexOf(actionId) !== -1) {
                                    if (actionId === "accountTransactions") {
                                        // Reached directly from the sidebar,
                                        // not via a specific account's
                                        // "Transactions" button — let the
                                        // page fall back to its own default
                                        // account, and return here (not to
                                        // Manage Accounts) on back.
                                        root.accountTransactionsIndex = -1
                                        root.accountTransactionsReturnViewId = "transactionsManagement"
                                    }
                                    root.currentViewId = actionId
                                }
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

    // ── Update banner (#117) ─────────────────────────────────────────────────
    // Above the page content, when a newer version of the channel exists, but
    // not on the Updates page itself. The pages and the overlays start below
    // it; with no banner, its height is 0.
    Rectangle {
        id: updateBanner
        anchors { top: parent.top; left: sidebarDivider.right; right: parent.right }
        height: UpdateController.bannerVisible && root.currentViewId !== "updates" ? 40 : 0
        visible: height > 0
        color: "#1e3a5f"

        RowLayout {
            anchors { fill: parent; leftMargin: 20; rightMargin: 12 }
            spacing: 12

            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: Application.displayName + " " + UpdateController.latestVersion + " is available."
                font.pixelSize: 14
                color: "#cdd6f4"
            }
            PillButton {
                text: "See the versions"
                variant: "primary"
                fontSize: 13
                Layout.preferredHeight: 28
                onClicked: root.currentViewId = "updates"
            }
            GhostButton {
                text: "×"
                fontSize: 15
                onClicked: UpdateController.dismissBanner()
            }
        }
    }

    // ── Page content ─────────────────────────────────────────────────────────
    StackLayout {
        anchors {
            top: updateBanner.bottom; bottom: parent.bottom
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
        DatabaseManagementPage        { prodDatabasePath: root.prodDatabasePath }
        UpdatesPage                   {}
    }

    // ── Overlay — Import Transactions (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ────────────
    Loader {
        anchors {
            top: updateBanner.bottom; bottom: parent.bottom
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
            top: updateBanner.bottom; bottom: parent.bottom
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
            top: updateBanner.bottom; bottom: parent.bottom
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
            // Management) — remember which account and that back should
            // return here, then switch views.
            onViewTransactionsRequested: (accountIndex) => {
                root.accountTransactionsIndex = accountIndex
                root.accountTransactionsReturnViewId = "manageAccounts"
                root.currentViewId = "accountTransactions"
            }
        }
    }

    // ── Overlay — View & Edit Transactions (Transactions category's own
    //    sidebar action *and* reachable from Manage Accounts' per-row
    //    "Transactions" button — see the sidebar's onActionTriggered handler
    //    and accountsPageComponent's onViewTransactionsRequested above).
    //    Lazily loaded/unloaded via Loader.active. ───────────────────────────
    Loader {
        anchors {
            top: updateBanner.bottom; bottom: parent.bottom
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
            // Back arrow's target depends on how this page was reached — see
            // accountTransactionsReturnViewId's own doc comment above.
            onCloseRequested: root.currentViewId = root.accountTransactionsReturnViewId
        }
    }

    // ── Overlay — Account Types & Categories (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. ──────────
    Loader {
        anchors {
            top: updateBanner.bottom; bottom: parent.bottom
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

    // ── Overlay — Account Details (see SidebarCategory.actionTriggered
    //    handling above). Lazily loaded/unloaded via Loader.active. Own
    //    sidebar action under Overview (not Account Management — this is a
    //    read-only analysis view, not an admin/CRUD page, see
    //    ARCHITECTURE.MD § Account History for the naming/placement note).
    //    Backing QML type/file is still AccountHistoryPage.qml — same
    //    "sidebar label renamed, class name unchanged" precedent as
    //    SnapshotImportController backing "Import Transactions". ──────────
    Loader {
        anchors {
            top: updateBanner.bottom; bottom: parent.bottom
            left: sidebarDivider.right; right: parent.right
        }
        active: root.currentViewId === "accountDetails"
        visible: active
        sourceComponent: accountHistoryPageComponent
    }

    Component {
        id: accountHistoryPageComponent
        AccountHistoryPage {
            onCloseRequested: root.currentViewId = "overview"
        }
    }

    // ── Database upgrade notice ──────────────────────────────────────────────
    // Opens at start-up when databaseNotice is set (see above): the user sees
    // once that the database changed, where the copy from before is, and how
    // many duplicate transactions were removed. Same look as ConfirmDialog.
    Popup {
        id: databaseNoticePopup
        anchors.centerIn: Overlay.overlay
        width: 460
        padding: 20
        modal: true

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: "Database upgraded"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.databaseNotice
                font.pixelSize: 14
                color: "#a6adc8"
            }
            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                PillButton { text: "OK"; variant: "primary"; onClicked: databaseNoticePopup.close() }
            }
        }

        Component.onCompleted: {
            if (root.databaseNotice.length > 0) {
                open()
            }
        }
    }
}
