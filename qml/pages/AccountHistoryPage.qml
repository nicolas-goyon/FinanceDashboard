pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Account Details — Overview → "Account Details" ────────────────────────────
// Read-only per-account view: current balance, a day-by-day balance-over-time
// line chart, and the account's transaction list — the "not to manage, but to
// see history" page from the product request. Deliberately has no edit/
// delete/add actions anywhere on it; that's AccountsManagePage.qml /
// AccountTransactionsPage.qml's job (reached from Manage Accounts). Lives
// under the **Overview** sidebar category (moved here from Account
// Management on product feedback: this is a day-to-day analysis view, not
// an admin/CRUD operation, so it belongs with the other "look at my data"
// pages rather than the account-admin category) — reached directly from the
// sidebar via its own "Account Details" action, not drilled into from
// Manage Accounts, since it's a standalone view over any account (has its
// own account picker), not tied to a specific row's button. File/QML type
// name is still `AccountHistoryPage` — only the sidebar label/id changed,
// same "renamed label, unchanged class name" precedent as
// `SnapshotImportController` backing "Import Transactions" — see
// ARCHITECTURE.MD § Account History.
//
// Backed by AccountController (QML_SINGLETON) directly, same as
// AccountTransactionsPage.qml — no dedicated controller for this page, since
// it only reads account-domain data that already lives on AccountController
// (accountBalanceHistory()/accountBalanceSummary(), see that class).
//
// Chart is the same GraphsView/DateTimeAxis/ValueAxis/LineSeries recipe as
// SavingsPage.qml's "Balance over time" chart (verified against that page,
// not invented — see ARCHITECTURE.MD header note on checking
// QtGraphsExamples/doc.qt.io first), with one difference: the LineSeries here
// is populated imperatively (append()/clear() in rebuildChart()) rather than
// once in Component.onCompleted, because the account picker can change which
// account's history is plotted without the page itself being recreated.

Item {
    id: page

    signal closeRequested()

    property int selectedAccountIndex: AccountController.accounts.length > 0 ? 0 : -1

    readonly property var account:
        page.selectedAccountIndex >= 0 && page.selectedAccountIndex < AccountController.accounts.length
            ? AccountController.accounts[page.selectedAccountIndex] : null

    property var summary: ({})
    property var history: []
    property var rows: []
    // { income: [{ category, totalRaw, totalText, subcategories: [...] }, ...],
    //   expenses: [ ...same shape... ] } — see
    //   AccountController::accountCategoryBreakdown()'s doc comment. Backs the
    //   two "Income by category" / "Expenses by category" nested pie charts.
    property var breakdown: ({})

    // Y-axis scale toggle for the balance chart (product request: "a way to
    // change the balance over time to change the scale of it"). "Auto" keeps
    // the existing tight-fit behavior (balanceAxisMin/Max below, padded ±10%,
    // capped at 0 for accounts that never go negative). "From 0" pins the
    // axis minimum to 0 so growth reads against a fixed zero baseline —
    // trading off clipping the line if the balance dips negative for an
    // easier "how big is the change" read. Doesn't touch balanceAxisMin
    // itself; only the GraphsView's axisY.min binding switches on it.
    property bool axisStartAtZero: false

    // X-axis time-range presets for the balance chart — "7d"/"1m"/"6m"/"1y"/
    // "all"/"custom". Only changes what window of the already-fully-plotted
    // `balanceSeries` the DateTimeAxis shows (axisRangeMinMs/MaxMs below) —
    // the series itself always holds every point (see rebuildChart()); this
    // never re-fetches or filters `page.history` itself. Rolling, not
    // calendar-aligned: "Last 6 months" is 6 months back from the account's
    // *last* transaction date (`maxTimeMs`), not from today — makes sense
    // for accounts whose history doesn't extend to the present.
    property string chartRange: "all" // "7d" | "1m" | "6m" | "1y" | "all" | "custom"
    // Custom range bounds — plain "yyyy-MM-dd" text, same convention as the
    // balance-checkpoint date field on AccountsManagePage.qml ("No
    // date-picker widget in this app yet... so the date is plain 'yyyy-MM-dd'
    // text" — nothing invented here either).
    property string customStartIso: ""
    property string customEndIso: ""

    function monthsBeforeMs(anchorMs, n) {
        var d = new Date(anchorMs)
        d.setMonth(d.getMonth() - n)
        return d.getTime()
    }

    // Raw (unpadded) axis window for the selected preset — anchored to the
    // account's own last data point, not "today".
    readonly property double rawRangeMinMs: {
        switch (page.chartRange) {
        case "7d":     return page.maxTimeMs - 7 * 86400000
        case "1m":     return page.monthsBeforeMs(page.maxTimeMs, 1)
        case "6m":     return page.monthsBeforeMs(page.maxTimeMs, 6)
        case "1y":     return page.monthsBeforeMs(page.maxTimeMs, 12)
        case "custom": return page.customStartIso.length > 0 ? Date.parse(page.customStartIso) : page.minTimeMs
        default:       return page.minTimeMs // "all"
        }
    }
    readonly property double rawRangeMaxMs:
        page.chartRange === "custom" && page.customEndIso.length > 0
            ? Date.parse(page.customEndIso) : page.maxTimeMs
    // Same "pad a degenerate range by one day" guard as maxTimeMs above
    // (docs/charting-gotchas.md — DateTimeAxis min === max crashes natively).
    // isNaN guards an unparseable custom date string.
    readonly property double axisRangeMinMs: isNaN(page.rawRangeMinMs) ? page.minTimeMs : page.rawRangeMinMs
    readonly property double axisRangeMaxMs: {
        var rawMax = isNaN(page.rawRangeMaxMs) ? page.maxTimeMs : page.rawRangeMaxMs
        return rawMax > page.axisRangeMinMs ? rawMax : page.axisRangeMinMs + 86400000
    }

    function reload() {
        if (page.selectedAccountIndex >= 0) {
            page.summary = AccountController.accountBalanceSummary(page.selectedAccountIndex)
            page.history = AccountController.accountBalanceHistory(page.selectedAccountIndex)
            page.rows = AccountController.transactionsForAccount(page.selectedAccountIndex)
            page.breakdown = AccountController.accountCategoryBreakdown(page.selectedAccountIndex)
        } else {
            page.summary = ({})
            page.history = []
            page.rows = []
            page.breakdown = ({})
        }
        rebuildChart()
        rebuildBreakdownPies()
    }

    function rebuildChart() {
        balanceSeries.clear()
        for (var i = 0; i < page.history.length; i++) {
            balanceSeries.append(page.history[i].timeMs, page.history[i].balance)
        }
    }

    // Rebuilds both income/expense pies from page.breakdown — page-level
    // (not each PieSeries's own Component.onCompleted) for the same reason
    // rebuildChart() above is: a child's Component.onCompleted runs before its
    // parent's, so a series filled that way at construction time would run
    // before `page.breakdown` has ever been set — see docs/charting-gotchas.md.
    function rebuildBreakdownPies() {
        buildCategoryRings(incomeCategoryPie,  incomeSubcategoryPie,  page.breakdown.income   ? page.breakdown.income   : [])
        buildCategoryRings(expenseCategoryPie, expenseSubcategoryPie, page.breakdown.expenses ? page.breakdown.expenses : [])
    }

    // Builds one 2-level pie as *two independently-sized, concentric
    // PieSeries* sharing the same GraphsView — categoryRing (inner, smaller
    // pieSize/holeSize) and subcategoryRing (outer, larger pieSize/holeSize
    // starting right where the inner ring ends, see the QML pieSize/holeSize
    // values). Deliberately not Qt Graphs' native PieSlice sub-slice feature
    // (PieSlice.append() sub-slices — see doc.qt.io/qt-6/qml-qtgraphs-
    // pieslice.html): that feature exists and works, but nothing in Qt's
    // docs (QML or C++ QPieSlice/QPieSeries) specifies whether a parent
    // slice's own ring renders as the inner or outer ring relative to its
    // sub-slices — only `subSlicesRatio` (their *relative size*) is
    // documented. Two separately-configured PieSeries give explicit,
    // guaranteed control over which ring is inner vs. outer instead of
    // relying on that unspecified behavior — see docs/charting-gotchas.md.
    //
    // Both series iterate the exact same `categories` array in the exact
    // same order (categoryRing: one slice per category; subcategoryRing: one
    // slice per subcategory, grouped by category) — since a category's own
    // total always equals the sum of its subcategories' totals (by
    // construction in AccountController::accountCategoryBreakdown()), the two
    // rings' cumulative percentages land on the same angles, so the outer
    // ring's subcategory slices always line up under their parent category's
    // slice in the inner ring, sunburst-style, with no manual angle math.
    function buildCategoryRings(categoryRing, subcategoryRing, categories) {
        categoryRing.clear()
        subcategoryRing.clear()
        for (var i = 0; i < categories.length; i++) {
            var cat = categories[i]
            var catSlice = categoryRing.append(cat.category, cat.totalRaw)
            catSlice.color = CategoryController.categoryColor(cat.category) || "#6c7086"
            catSlice.labelVisible = true
            for (var j = 0; j < cat.subcategories.length; j++) {
                var sub = cat.subcategories[j]
                var subSlice = subcategoryRing.append(sub.subcategory, sub.totalRaw)
                subSlice.color = CategoryController.subcategoryColor(cat.category, sub.subcategory) || "#6c7086"
                // Was missing — the outer ring showed no labels at all, only
                // the inner category ring did (product feedback). The
                // series' own angleSpanVisibleLimit (set in QML below) hides
                // labels on the thinnest slices so many subcategories don't
                // pile their label arms on top of each other.
                subSlice.labelVisible = true
            }
        }
    }

    Component.onCompleted: page.reload()
    onSelectedAccountIndexChanged: page.reload()

    // Crash fix: an account with only one transaction *date* (very common —
    // e.g. a single imported/edited row) makes minTimeMs === maxTimeMs below
    // if left unpadded. DateTimeAxis's docs don't forbid an equal min/max,
    // but Qt Graphs' automatic tick-layout ("If [tickInterval] is 0 or lower,
    // axis will automatically calculate appropriate amount of ticks") almost
    // certainly divides by the axis range internally — a zero-width range
    // reproducibly crashed the app with no QML-visible error (a native
    // crash bypasses QML's own warning/error console entirely, unlike a JS
    // exception). Padding by one day whenever the raw range collapses to
    // zero keeps the axis strictly positive-width in every case.
    readonly property double minTimeMs: page.history.length > 0 ? page.history[0].timeMs : 0
    readonly property double maxTimeMsRaw: page.history.length > 0 ? page.history[page.history.length - 1].timeMs : 86400000
    readonly property double maxTimeMs: page.maxTimeMsRaw > page.minTimeMs ? page.maxTimeMsRaw : page.minTimeMs + 86400000
    readonly property double balanceAxisMin: {
        if (page.history.length === 0) return 0
        var m = page.history[0].balance
        for (var i = 1; i < page.history.length; i++) m = Math.min(m, page.history[i].balance)
        return Math.min(m * 1.1, 0)
    }
    readonly property double balanceAxisMax: {
        if (page.history.length === 0) return 1
        var m = page.history[0].balance
        for (var i = 1; i < page.history.length; i++) m = Math.max(m, page.history[i].balance)
        return m > 0 ? m * 1.1 : 1
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            PageHeader {
                showBack: true
                title: "Account Details" + (page.account !== null ? " — " + page.account.name : "")
                onBackClicked: page.closeRequested()

                ComboBox {
                    Layout.preferredWidth: 240
                    model: AccountController.accounts
                    textRole: "name"
                    currentIndex: page.selectedAccountIndex
                    onActivated: page.selectedAccountIndex = currentIndex
                }
            }

            Text {
                visible: AccountController.accounts.length === 0
                text: "No accounts yet — add one from Account Management → Manage Accounts."
                font.pixelSize: 15
                color: "#f38ba8"
            }

            ColumnLayout {
                visible: page.account !== null
                Layout.fillWidth: true
                spacing: 16

                RowLayout {
                    id: kpiRow
                    Layout.fillWidth: true
                    spacing: 12
                    // Three cards must be exactly equal width. `Layout.fillWidth: true`
                    // alone (the previous code) leaves each card's *preferred* width at
                    // its own implicit size before the fill share is added on top —
                    // fine when all three happen to have the same implicit width, but
                    // QtQuick Layouts' remainder handling for the leftover fractional
                    // pixel (row width isn't always divisible by 3) doesn't split it
                    // symmetrically, so the middle card reproducibly rendered a hair
                    // narrower than its neighbors. Binding an explicit, identical
                    // `Layout.preferredWidth` to all three removes the ambiguity.
                    readonly property real cardWidth: (kpiRow.width - kpiRow.spacing * 2) / 3
                    KpiCard {
                        Layout.preferredWidth: kpiRow.cardWidth
                        Layout.fillWidth: true
                        label: "Current balance"
                        value: page.summary.currentBalanceText ? page.summary.currentBalanceText : "—"
                        sub: page.summary.asOfDateText ? ("as of " + page.summary.asOfDateText) : ""
                    }
                    KpiCard {
                        Layout.preferredWidth: kpiRow.cardWidth
                        Layout.fillWidth: true
                        label: "Transactions"
                        value: page.summary.transactionCount !== undefined ? String(page.summary.transactionCount) : "0"
                        // A non-empty sub line, not "" — KpiCard hides an empty sub Text
                        // (visible: root.sub.length > 0), and QtQuick Layouts excludes
                        // invisible children from a ColumnLayout's implicit-height
                        // calculation, so an empty sub made this card two lines tall
                        // instead of three — a hair shorter than its neighbors (which
                        // both have a real sub line). "All time" is also accurate: this
                        // count is the account's whole history, not scoped to any
                        // selected chart range.
                        sub: "All time"
                    }
                    KpiCard {
                        Layout.preferredWidth: kpiRow.cardWidth
                        Layout.fillWidth: true
                        label: "Type / Category"
                        value: page.account !== null ? page.account.type : "—"
                        sub: page.account !== null ? page.account.category : ""
                    }
                }

                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: 410
                    title: "Balance over time (€) — computed from every transaction"

                    // Chart controls — direct children of ChartCard's content Item
                    // (not a Layout), positioned by anchors like the GraphsView/
                    // EmptyState below it, same convention the rest of this file
                    // already uses for ChartCard content. Grouped in one
                    // ColumnLayout (itself just anchored, not fill — its own
                    // implicit height changes depending on whether the custom-date
                    // row is showing) so the GraphsView below can anchor to
                    // whichever height that column ends up needing.
                    ColumnLayout {
                        id: chartControls
                        anchors { top: parent.top; left: parent.left; right: parent.right }
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text { text: "RANGE"; font.pixelSize: 12; font.bold: true; color: "#6c7086" }
                            Item { Layout.fillWidth: true }
                            PillButton {
                                text: "7D"; fontSize: 12; horizontalPadding: 10; implicitHeight: 24
                                variant: page.chartRange === "7d" ? "primary" : "neutral"
                                onClicked: page.chartRange = "7d"
                            }
                            PillButton {
                                text: "1M"; fontSize: 12; horizontalPadding: 10; implicitHeight: 24
                                variant: page.chartRange === "1m" ? "primary" : "neutral"
                                onClicked: page.chartRange = "1m"
                            }
                            PillButton {
                                text: "6M"; fontSize: 12; horizontalPadding: 10; implicitHeight: 24
                                variant: page.chartRange === "6m" ? "primary" : "neutral"
                                onClicked: page.chartRange = "6m"
                            }
                            PillButton {
                                text: "1Y"; fontSize: 12; horizontalPadding: 10; implicitHeight: 24
                                variant: page.chartRange === "1y" ? "primary" : "neutral"
                                onClicked: page.chartRange = "1y"
                            }
                            PillButton {
                                text: "From start"; fontSize: 12; horizontalPadding: 10; implicitHeight: 24
                                variant: page.chartRange === "all" ? "primary" : "neutral"
                                onClicked: page.chartRange = "all"
                            }
                            PillButton {
                                text: "Custom"; fontSize: 12; horizontalPadding: 10; implicitHeight: 24
                                variant: page.chartRange === "custom" ? "primary" : "neutral"
                                onClicked: page.chartRange = "custom"
                            }
                        }

                        // Plain "yyyy-MM-dd" text fields, not a date-picker widget —
                        // see page.customStartIso's doc comment above.
                        RowLayout {
                            Layout.fillWidth: true
                            visible: page.chartRange === "custom"
                            spacing: 6
                            Item { Layout.fillWidth: true }
                            Text { text: "From"; font.pixelSize: 12; color: "#6c7086" }
                            TextField {
                                Layout.preferredWidth: 110
                                implicitHeight: 24
                                font.pixelSize: 12
                                placeholderText: "yyyy-MM-dd"
                                text: page.customStartIso
                                onEditingFinished: page.customStartIso = text.trim()
                            }
                            Text { text: "to"; font.pixelSize: 12; color: "#6c7086" }
                            TextField {
                                Layout.preferredWidth: 110
                                implicitHeight: 24
                                font.pixelSize: 12
                                placeholderText: "yyyy-MM-dd"
                                text: page.customEndIso
                                onEditingFinished: page.customEndIso = text.trim()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text { text: "Y AXIS"; font.pixelSize: 12; font.bold: true; color: "#6c7086" }
                            Item { Layout.fillWidth: true }
                            PillButton {
                                text: "Auto"
                                variant: page.axisStartAtZero ? "neutral" : "primary"
                                fontSize: 12
                                horizontalPadding: 10
                                implicitHeight: 24
                                onClicked: page.axisStartAtZero = false
                            }
                            PillButton {
                                text: "From 0"
                                variant: page.axisStartAtZero ? "primary" : "neutral"
                                fontSize: 12
                                horizontalPadding: 10
                                implicitHeight: 24
                                onClicked: page.axisStartAtZero = true
                            }
                        }
                    }

                    GraphsView {
                        anchors { top: chartControls.bottom; topMargin: 6; left: parent.left; right: parent.right; bottom: parent.bottom }
                        visible: page.history.length > 0
                        theme: GraphsTheme {
                            colorScheme: GraphsTheme.ColorScheme.Dark
                            plotAreaBackgroundVisible: true
                            plotAreaBackgroundColor: "#181825"
                            backgroundVisible: false
                            gridVisible: true
                            grid.mainColor: "#313244"
                            grid.subColor: "#313244"
                            axisX.mainColor: "#313244"
                            axisX.labelTextColor: "#6c7086"
                            axisY.mainColor: "#313244"
                            axisY.labelTextColor: "#6c7086"
                            labelTextColor: "#6c7086"
                            labelFont.pixelSize: 12
                        }

                        axisX: DateTimeAxis {
                            min: new Date(page.axisRangeMinMs)
                            max: new Date(page.axisRangeMaxMs)
                            labelFormat: "MMM yy"
                        }
                        axisY: ValueAxis {
                            min: page.axisStartAtZero ? 0 : page.balanceAxisMin
                            max: page.balanceAxisMax
                            labelFormat: "%.0f"
                        }

                        LineSeries {
                            id: balanceSeries
                            name: page.account !== null ? page.account.name : ""
                            color: "#89b4fa"
                            width: 2
                            pointDelegate: Component {
                                HoverPoint {
                                    seriesName: page.account !== null ? page.account.name : ""
                                    pointColor: "#89b4fa"
                                }
                            }
                        }
                    }

                    EmptyState {
                        visible: page.history.length === 0
                        anchors.centerIn: parent
                        message: "No dated transactions to chart yet for this account."
                    }
                }

                // ── Income / expenses by category — two 2-level nested pies ─────
                // Income and expense are derived per-transaction from
                // credit/debit (not a category-level tag — see
                // AccountController::accountCategoryBreakdown()'s doc comment),
                // so the same category can appear in both pies. Each pie is
                // *two separate, stacked `GraphsView`s* (one `PieSeries` each —
                // categoryRing inner, subcategoryRing outer), not two
                // `PieSeries` sharing one `GraphsView`. That single-view
                // version had both rings' `onClicked` go dead — every other
                // pie in this app (Import report, Cash Flow) is one
                // `PieSeries` per `GraphsView`, the only configuration Qt
                // Graphs actually demonstrates anywhere, so this keeps each
                // ring in its own isolated, individually-proven-clickable
                // view instead of guessing at how two series interact inside
                // one. See docs/charting-gotchas.md.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    ChartCard {
                        Layout.fillWidth: true
                        implicitHeight: 320
                        title: "Income by category"

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            EmptyState {
                                visible: !page.breakdown.income || page.breakdown.income.length === 0
                                message: "No income transactions for this account yet."
                            }

                            // Plain Item host, not a Layout — the two GraphsViews
                            // inside are stacked via anchors.fill: parent (exactly
                            // overlapping), which anchors on a direct Layout child
                            // can't do (see the "direct anchored child of a plain
                            // Item" convention used everywhere else on this page).
                            Item {
                                id: incomePieHost
                                visible: page.breakdown.income && page.breakdown.income.length > 0
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                // Inner ring — categories. Own GraphsView, carries
                                // the shared plot-area background for both rings.
                                GraphsView {
                                    anchors.fill: parent
                                    theme: GraphsTheme {
                                        colorScheme: GraphsTheme.ColorScheme.Dark
                                        plotAreaBackgroundVisible: true
                                        plotAreaBackgroundColor: "#181825"
                                        backgroundVisible: false
                                        labelTextColor: "#cdd6f4"
                                        labelFont.pixelSize: 13
                                    }
                                    PieSeries {
                                        id: incomeCategoryPie
                                        pieSize: 0.55
                                        holeSize: 0.28
                                        onClicked: (slice) => {
                                            incomePieInfo.text = "Category — " + slice.label + "  —  "
                                                + slice.value.toLocaleString(Qt.locale("fr-FR"), 'f', 0)
                                                + " €  (" + Math.round(slice.percentage * 100) + " %)"
                                        }
                                    }
                                }

                                // Outer ring — subcategories, starts right where
                                // the category ring ends (holeSize 0.58 vs. its
                                // pieSize 0.55 — a thin 0.03 gap for separation).
                                // Own GraphsView stacked exactly on top; no
                                // plot-area background of its own (the inner
                                // view's is directly behind it) so it doesn't
                                // paint a second opaque layer over the first.
                                GraphsView {
                                    anchors.fill: parent
                                    theme: GraphsTheme {
                                        colorScheme: GraphsTheme.ColorScheme.Dark
                                        plotAreaBackgroundVisible: false
                                        backgroundVisible: false
                                        labelTextColor: "#cdd6f4"
                                        labelFont.pixelSize: 11
                                    }
                                    PieSeries {
                                        id: incomeSubcategoryPie
                                        pieSize: 0.92
                                        holeSize: 0.58
                                        // Hides labels on the thinnest slices so many
                                        // subcategories don't pile their label arms
                                        // on top of each other (Qt 6.10+ property,
                                        // verified on doc.qt.io before using it).
                                        angleSpanVisibleLimit: 10
                                        onClicked: (slice) => {
                                            incomePieInfo.text = "Subcategory — " + slice.label + "  —  "
                                                + slice.value.toLocaleString(Qt.locale("fr-FR"), 'f', 0)
                                                + " €  (" + Math.round(slice.percentage * 100) + " %)"
                                        }
                                    }
                                }
                            }

                            Text {
                                id: incomePieInfo
                                Layout.alignment: Qt.AlignHCenter
                                visible: page.breakdown.income && page.breakdown.income.length > 0
                                text: "Click a category (inner ring) or subcategory (outer ring) slice for details"
                                font.pixelSize: 13
                                color: "#6c7086"
                                wrapMode: Text.WordWrap
                                Layout.maximumWidth: parent.width
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }

                    ChartCard {
                        Layout.fillWidth: true
                        implicitHeight: 320
                        title: "Expenses by category"

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            EmptyState {
                                visible: !page.breakdown.expenses || page.breakdown.expenses.length === 0
                                message: "No expense transactions for this account yet."
                            }

                            Item {
                                id: expensePieHost
                                visible: page.breakdown.expenses && page.breakdown.expenses.length > 0
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                // Inner ring — categories.
                                GraphsView {
                                    anchors.fill: parent
                                    theme: GraphsTheme {
                                        colorScheme: GraphsTheme.ColorScheme.Dark
                                        plotAreaBackgroundVisible: true
                                        plotAreaBackgroundColor: "#181825"
                                        backgroundVisible: false
                                        labelTextColor: "#cdd6f4"
                                        labelFont.pixelSize: 13
                                    }
                                    PieSeries {
                                        id: expenseCategoryPie
                                        pieSize: 0.55
                                        holeSize: 0.28
                                        onClicked: (slice) => {
                                            expensePieInfo.text = "Category — " + slice.label + "  —  "
                                                + slice.value.toLocaleString(Qt.locale("fr-FR"), 'f', 0)
                                                + " €  (" + Math.round(slice.percentage * 100) + " %)"
                                        }
                                    }
                                }

                                // Outer ring — subcategories.
                                GraphsView {
                                    anchors.fill: parent
                                    theme: GraphsTheme {
                                        colorScheme: GraphsTheme.ColorScheme.Dark
                                        plotAreaBackgroundVisible: false
                                        backgroundVisible: false
                                        labelTextColor: "#cdd6f4"
                                        labelFont.pixelSize: 11
                                    }
                                    PieSeries {
                                        id: expenseSubcategoryPie
                                        pieSize: 0.92
                                        holeSize: 0.58
                                        angleSpanVisibleLimit: 10
                                        onClicked: (slice) => {
                                            expensePieInfo.text = "Subcategory — " + slice.label + "  —  "
                                                + slice.value.toLocaleString(Qt.locale("fr-FR"), 'f', 0)
                                                + " €  (" + Math.round(slice.percentage * 100) + " %)"
                                        }
                                    }
                                }
                            }

                            Text {
                                id: expensePieInfo
                                Layout.alignment: Qt.AlignHCenter
                                visible: page.breakdown.expenses && page.breakdown.expenses.length > 0
                                text: "Click a category (inner ring) or subcategory (outer ring) slice for details"
                                font.pixelSize: 13
                                color: "#6c7086"
                                wrapMode: Text.WordWrap
                                Layout.maximumWidth: parent.width
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }

                // ── Transaction history table (read-only, no edit/delete) ───────
                // Responsive fix: the table's 7 columns have a fixed combined width
                // (see `txCol.rowWidth` below) — too wide to ever meaningfully
                // "shrink to fit" a narrow window without truncating every column.
                // Previously each row Rectangle used `Layout.fillWidth: true` with
                // no ScrollView, so on a narrow window the fixed-width Text columns
                // simply rendered past the Rectangle's (and therefore the card's)
                // right edge with nothing to clip or scroll it — the reported
                // overflow bug. Fix: give the header/row Rectangles an explicit
                // fixed `Layout.preferredWidth: txCol.rowWidth` instead of
                // fillWidth, and host `txCol` inside a `ScrollView` — per
                // doc.qt.io's ScrollView "Sizing" section, a single-child
                // ScrollView derives its scrollable content size from that child's
                // implicit size, so `txCol` (implicitWidth = rowWidth, from its
                // now fixed-width children) becomes horizontally scrollable
                // whenever the card is narrower than the table, and shows no
                // scrollbar at all when there's room to spare.
                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: txCol.implicitHeight + 50
                    title: "Transaction history"

                    ScrollView {
                        anchors.fill: parent
                        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                        ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                        ColumnLayout {
                            id: txCol
                            spacing: 0

                            // DATE 90 + NAME 300 + gap 16 + DEBIT 90 + CREDIT 90 +
                            // BALANCE 100 + CATEGORY 130 + SUBCATEGORY 130 = 946,
                            // plus 7 inter-column gaps × 8 spacing = 56, plus the
                            // row's own 10+10 left/right margins = 20. 946+56+20.
                            readonly property int rowWidth: 1022

                            Rectangle {
                                Layout.preferredWidth: txCol.rowWidth
                                Layout.preferredHeight: 28
                                color: "#181825"
                                radius: 4
                                RowLayout {
                                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                    spacing: 8
                                    Text { Layout.preferredWidth: 90;  text: "DATE";        font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                    Text { Layout.preferredWidth: 300; text: "NAME";        font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                    Item { Layout.preferredWidth: 16 }
                                    Text { Layout.preferredWidth: 90;  text: "DEBIT";       font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 90;  text: "CREDIT";      font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 100; text: "BALANCE";     font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 130; text: "CATEGORY";    font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                    Text { Layout.preferredWidth: 130; text: "SUBCATEGORY"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                }
                            }

                            EmptyState {
                                visible: page.rows.length === 0
                                Layout.topMargin: 10
                                Layout.preferredWidth: txCol.rowWidth
                                message: "No transactions saved for this account yet."
                            }

                            Repeater {
                                model: page.rows
                                delegate: Rectangle {
                                    id: txRow
                                    required property var modelData
                                    required property int index
                                    Layout.preferredWidth: txCol.rowWidth
                                    Layout.preferredHeight: 36
                                    color: index % 2 === 0 ? "transparent" : "#181825"

                                    RowLayout {
                                        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                        spacing: 8
                                        Text { Layout.preferredWidth: 90; text: txRow.modelData.operationDate; font.pixelSize: 14; color: "#6c7086" }
                                        Text {
                                            Layout.preferredWidth: 300
                                            text: txRow.modelData.name.length > 0 ? txRow.modelData.name : "—"
                                            font.pixelSize: 15
                                            color: "#cdd6f4"
                                            elide: Text.ElideRight
                                        }
                                        Item { Layout.preferredWidth: 16 }
                                        Text { Layout.preferredWidth: 90;  text: txRow.modelData.debitText;  font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                        Text { Layout.preferredWidth: 90;  text: txRow.modelData.creditText; font.pixelSize: 15; color: "#a6e3a1"; horizontalAlignment: Text.AlignRight }
                                        Text { Layout.preferredWidth: 100; text: txRow.modelData.balanceText; font.pixelSize: 14; color: "#6c7086"; horizontalAlignment: Text.AlignRight }

                                        Rectangle {
                                            Layout.preferredWidth: 130
                                            visible: txRow.modelData.category.length > 0
                                            Layout.preferredHeight: 20
                                            radius: 10
                                            color: Qt.rgba(1, 1, 1, 0.08)
                                            border.color: CategoryController.categoryColor(txRow.modelData.category) || "#6c7086"
                                            Text {
                                                anchors.centerIn: parent
                                                text: txRow.modelData.category
                                                font.pixelSize: 12
                                                color: CategoryController.categoryColor(txRow.modelData.category) || "#6c7086"
                                                elide: Text.ElideRight
                                                width: Math.min(implicitWidth, 118)
                                            }
                                        }
                                        Text { Layout.preferredWidth: 130; visible: txRow.modelData.category.length === 0; text: "—"; font.pixelSize: 14; color: "#6c7086" }

                                        Rectangle {
                                            Layout.preferredWidth: 130
                                            visible: txRow.modelData.subcategory.length > 0
                                            Layout.preferredHeight: 20
                                            radius: 10
                                            color: Qt.rgba(1, 1, 1, 0.08)
                                            border.color: CategoryController.subcategoryColor(txRow.modelData.category, txRow.modelData.subcategory) || "#6c7086"
                                            Text {
                                                anchors.centerIn: parent
                                                text: txRow.modelData.subcategory
                                                font.pixelSize: 12
                                                color: CategoryController.subcategoryColor(txRow.modelData.category, txRow.modelData.subcategory) || "#6c7086"
                                                elide: Text.ElideRight
                                                width: Math.min(implicitWidth, 118)
                                            }
                                        }
                                        Text { Layout.preferredWidth: 130; visible: txRow.modelData.subcategory.length === 0; text: "—"; font.pixelSize: 14; color: "#6c7086" }
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
