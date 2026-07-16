import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── App shell ─────────────────────────────────────────────────────────────────
// Replaces the Qt Widgets MainWindow. Catppuccin Mocha palette:
//   Base #11111b · Mantle #181825 · Surface0 #1e1e2e · Surface1 #313244
//   Overlay0 #6c7086 · Text #cdd6f4 · Blue #89b4fa
//
// Layout: left sidebar (collapsible category list) instead of a top TabBar.
// Categories mirror the 6 PROJECT.md pages 1:1, plus a 7th "Other - To
// remove" category for anything not in the product spec (currently the
// unregistered qml/tabs/*.qml legacy files — see OtherLegacyPage.qml).
// Clicking a category label selects its page; clicking the small arrow
// expands/collapses its pre-filled action list (labels only — not wired to
// tabs yet, per instructions).

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 780
    title: "Finance Dashboard"
    color: "#11111b"

    property int currentIndex: 0

    // Two sidebar actions are wired up so far, both under Data Entry, each
    // opening a full-content overlay (via the Loaders below): "New Monthly
    // Snapshot" → ImportSnapshotPage.qml, "Manage Categories" →
    // CategoriesManagePage.qml. Every other pre-filled action is still inert.
    property bool showImportOverlay: false
    property bool showCategoriesOverlay: false

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
            label: "Data Entry",
            actions: ["New Monthly Snapshot", "Add Transaction", "History / Past Reports", "Annual Summary", "Manage Categories"]
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
                        onSelected: root.currentIndex = index
                        onActionTriggered: (action) => {
                            if (modelData.label === "Data Entry" && action === "New Monthly Snapshot") {
                                root.showCategoriesOverlay = false
                                root.showImportOverlay = true
                            } else if (modelData.label === "Data Entry" && action === "Manage Categories") {
                                root.showImportOverlay = false
                                root.showCategoriesOverlay = true
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
        visible: !root.showImportOverlay && !root.showCategoriesOverlay
        currentIndex: root.currentIndex

        OverviewPage    {}
        CashFlowPage    {}
        SavingsPage     {}
        InvestmentsPage {}
        ProjectsPage    {}
        DataEntryPage   {}
        OtherLegacyPage {}
    }

    // ── Overlay — New Monthly Snapshot import (see SidebarCategory.actionTriggered
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
}
