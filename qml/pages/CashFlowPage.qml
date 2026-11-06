pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Cash Flow — PROJECT.md page 2 ─────────────────────────────────────────────
// Money flow (Sankey) for a selected month · income vs expenses bars · rolling
// 3-month average line · category donut · stacked category bars · biggest
// expenses table. From the rows of the accounts of the net worth, without the
// internal transfers (#210): CashFlowController reads them again each time
// the page is shown.

Item {
    id: page

    CashFlowController { id: controller }

    // The label of the selected month, or "" when the index is outside the list (#69).
    readonly property string selectedMonthLabel:
        controller.selectedMonth >= 0 && controller.selectedMonth < controller.monthLabels.length
            ? controller.monthLabels[controller.selectedMonth] : ""

    // The name and colour of spending category i (controller.categories).
    function categoryName(i) {
        return i < controller.categories.length ? controller.categories[i].name : ""
    }
    function categoryColor(i) {
        return i < controller.categories.length ? controller.categories[i].color : Theme.overlay0
    }

    // Fills the charts that are not bound to a property. Also after a reload.
    function fill() {
        monthAxis.categories = controller.monthLabels
        stackedAxis.categories = controller.monthLabels
        incomeSet.values = controller.incomeValues
        expenseSet.values = controller.expenseValues
        rollingLine.clear()
        var t = controller.rollingTimestamps()
        var v = controller.rollingValues()
        for (var i = 0; i < t.length; i++)
            rollingLine.append(t[i], v[i])
        var sets = [stacked0, stacked1, stacked2, stacked3, stacked4, stacked5]
        for (var k = 0; k < sets.length; k++)
            sets[k].values = controller.categoryValues(k)
        categoryDonut.rebuild()
    }

    Component.onCompleted: page.fill()
    // The page stays loaded in the StackLayout: read the data again each time
    // it is shown.
    onVisibleChanged: {
        if (visible) {
            controller.reload()
            page.fill()
        }
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
                title: qsTr("Cash Flow")

                Text { text: qsTr("Month:"); font.pixelSize: 15; color: Theme.overlay0 }
                ComboBox {
                    id: monthSelector
                    Layout.preferredWidth: 120
                    model: controller.monthLabels
                    currentIndex: controller.selectedMonth
                    onActivated: (index) => controller.selectedMonth = index
                }
            }

            EmptyState {
                visible: !controller.hasData
                message: qsTr("No transaction in the accounts of your net worth yet. Import a statement in "
                    + "Transactions → Import Transactions.")
            }

            // ── Money flow (Sankey) for the selected month ────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 420
                title: qsTr("Money flow — %1 (income → budget → spending & savings)").arg(page.selectedMonthLabel)

                MoneyFlow {
                    id: moneyFlow
                    anchors.fill: parent
                    centreName: qsTr("Monthly budget")
                    inputs: controller.flowInputs
                    outputs: controller.flowOutputs
                }
            }

            // ── Income vs expenses ────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 340
                title: qsTr("Income vs expenses per month (€)")

                GraphsView {
                    anchors.fill: parent
                    theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                    axisY: ValueAxis {
                        min: 0
                        max: controller.barsMax
                        labelFormat: "%.0f"
                    }

                    BarSeries {
                        barDelegate: Component { HoverBar {} }

                        // Filled by page.fill().
                        axisX: BarCategoryAxis { id: monthAxis }
                        BarSet {
                            id: incomeSet
                            label: qsTr("Income")
                            color: Theme.green
                        }
                        BarSet {
                            id: expenseSet
                            label: qsTr("Expenses")
                            color: Theme.red
                        }
                    }
                }
            }

            // ── Rolling 3-month average ───────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 280
                title: qsTr("Rolling 3-month average spending (€)")

                GraphsView {
                    anchors.fill: parent
                    theme: GraphsTheme {
                        colorScheme: GraphsTheme.ColorScheme.Dark
                        plotAreaBackgroundVisible: true
                    }

                    axisX: DateTimeAxis {
                        min: new Date(controller.minTimeMs)
                        max: new Date(controller.maxTimeMs)
                        labelFormat: "MMM yy"
                    }
                    axisY: ValueAxis {
                        min: 0
                        max: controller.barsMax
                        labelFormat: "%.0f"
                    }

                    LineSeries {
                        id: rollingLine
                        name: qsTr("3-month avg")
                        color: Theme.blue
                        width: 2
                        // Filled by page.fill().
                        pointDelegate: Component {
                            HoverPoint {
                                seriesName: qsTr("3-month avg")
                                pointColor: Theme.blue
                            }
                        }
                    }
                }
            }

            // ── Donut + biggest expenses (selected month) ─────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ChartCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 360
                    title: qsTr("Spending by category — %1 — click a slice").arg(page.selectedMonthLabel)

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        GraphsView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                            PieSeries {
                                id: categoryDonut
                                // Without it the renderer emits no clicked signal (default false).
                                selectable: true
                                pieSize: 0.75
                                holeSize: 0.42

                                function rebuild() {
                                    clear()
                                    sliceInfo.text = qsTr("Click a slice for details")
                                    var s = controller.monthSlices
                                    for (var i = 0; i < s.length; i++) {
                                        var slice = append(s[i].label, s[i].value)
                                        slice.color = s[i].color
                                        // Without it, the slice gets the outline colour of the theme (#71).
                                        slice.borderColor = slice.color
                                        slice.labelVisible = true
                                    }
                                }

                                onClicked: (slice) => {
                                    sliceInfo.text = slice.label + "  —  "
                                        + slice.value.toLocaleString(Qt.locale("fr-FR"), 'f', 0)
                                        + " €  (" + Math.round(slice.percentage * 100) + " %)"
                                }
                            }

                            Connections {
                                target: controller
                                function onMonthChanged() { categoryDonut.rebuild() }
                            }
                        }
                        Text {
                            id: sliceInfo
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Click a slice for details")
                            font.pixelSize: 14
                            color: Theme.overlay0
                        }
                    }
                }

                ChartCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 360
                    title: qsTr("Biggest expenses — %1").arg(page.selectedMonthLabel)

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Repeater {
                            model: controller.biggestRows
                            delegate: Rectangle {
                                id: expenseRow
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                // Zebra parity matches every other striped list
                                // in the app (odd rows tinted) — this one used
                                // to be inverted, the only page that was.
                                color: index % 2 === 0 ? "transparent" : Theme.mantle
                                radius: 4

                                RowLayout {
                                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                    spacing: 10

                                    Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: expenseRow.modelData.color }
                                    Text {
                                        Layout.fillWidth: true
                                        text: expenseRow.modelData.label
                                        font.pixelSize: 15
                                        color: Theme.text
                                        elide: Text.ElideRight
                                    }
                                    Text { text: expenseRow.modelData.category; font.pixelSize: 14; color: Theme.overlay0 }
                                    Text { text: expenseRow.modelData.amount; font.pixelSize: 15; font.bold: true; color: Theme.text }
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            // ── Category drift (stacked) ──────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 340
                title: qsTr("Category spending across months (€)")

                GraphsView {
                    anchors.fill: parent
                    theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                    axisY: ValueAxis {
                        min: 0
                        max: controller.stackedMax
                        labelFormat: "%.0f"
                    }

                    // BarSets are static: a Repeater cannot fill a BarSeries, and the
                    // QtGraphsExamples bar charts declare each BarSet. There is one
                    // BarSet for each of the CashFlowController::kCategories (6)
                    // categories (the 5 categories with the most spending, then
                    // "Other"). Set i takes its label, colour and values from
                    // category i of the controller, so they cannot disagree (#72).
                    BarSeries {
                        barsType: BarSeries.BarsType.Stacked
                        barDelegate: Component { HoverBar {} }

                        // Filled by page.fill().
                        axisX: BarCategoryAxis { id: stackedAxis }

                        BarSet { id: stacked0; label: page.categoryName(0); color: page.categoryColor(0) }
                        BarSet { id: stacked1; label: page.categoryName(1); color: page.categoryColor(1) }
                        BarSet { id: stacked2; label: page.categoryName(2); color: page.categoryColor(2) }
                        BarSet { id: stacked3; label: page.categoryName(3); color: page.categoryColor(3) }
                        BarSet { id: stacked4; label: page.categoryName(4); color: page.categoryColor(4) }
                        BarSet { id: stacked5; label: page.categoryName(5); color: page.categoryColor(5) }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
