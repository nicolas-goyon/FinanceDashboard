pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Cash Flow — PROJECT.md page 2 ─────────────────────────────────────────────
// Money flow (Sankey) for a selected month · income vs expenses bars · rolling
// 3-month average line · category donut · stacked category bars · biggest
// expenses table    [MOCK DATA — CashFlowController]

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

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            PageHeader {
                title: "Cash Flow"
                mock: true

                Text { text: "Month:"; font.pixelSize: 15; color: Theme.overlay0 }
                ComboBox {
                    id: monthSelector
                    Layout.preferredWidth: 120
                    model: controller.monthLabels
                    currentIndex: controller.selectedMonth
                    onActivated: (index) => controller.selectedMonth = index
                }
            }

            // ── Money flow (Sankey) for the selected month ────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 420
                title: "Money flow — " + page.selectedMonthLabel
                       + " (income → budget → spending & savings)"

                MoneyFlow {
                    id: moneyFlow
                    anchors.fill: parent
                    centreName: "Monthly budget"
                    inputs: controller.flowInputs
                    outputs: controller.flowOutputs
                }
            }

            // ── Income vs expenses ────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 340
                title: "Income vs expenses per month (€)"

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

                        axisX: BarCategoryAxis {
                            Component.onCompleted: categories = controller.monthLabels
                        }
                        BarSet {
                            label: "Income"
                            color: Theme.green
                            Component.onCompleted: values = controller.incomeValues
                        }
                        BarSet {
                            label: "Expenses"
                            color: Theme.red
                            Component.onCompleted: values = controller.expenseValues
                        }
                    }
                }
            }

            // ── Rolling 3-month average ───────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 280
                title: "Rolling 3-month average spending (€)"

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
                        name: "3-month avg"
                        color: Theme.blue
                        width: 2

                        Component.onCompleted: {
                            var t = controller.rollingTimestamps()
                            var v = controller.rollingValues()
                            for (var i = 0; i < t.length; i++)
                                append(t[i], v[i])
                        }
                        pointDelegate: Component {
                            HoverPoint {
                                seriesName: "3-month avg"
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
                    title: "Spending by category — " + page.selectedMonthLabel
                           + " — click a slice"

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
                                    sliceInfo.text = "Click a slice for details"
                                    var s = controller.monthSlices
                                    for (var i = 0; i < s.length; i++) {
                                        var slice = append(s[i].label, s[i].value)
                                        slice.color = s[i].color
                                        // Without it, the slice gets the outline colour of the theme (#71).
                                        slice.borderColor = slice.color
                                        slice.labelVisible = true
                                    }
                                }
                                Component.onCompleted: rebuild()

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
                            text: "Click a slice for details"
                            font.pixelSize: 14
                            color: Theme.overlay0
                        }
                    }
                }

                ChartCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 360
                    title: "Biggest expenses — " + page.selectedMonthLabel

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
                title: "Category spending across months (€)"

                GraphsView {
                    anchors.fill: parent
                    theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                    axisY: ValueAxis {
                        min: 0
                        max: controller.barsMax
                        labelFormat: "%.0f"
                    }

                    // BarSets are static: a Repeater cannot fill a BarSeries, and the
                    // QtGraphsExamples bar charts declare each BarSet. There is one
                    // BarSet for each of the CashFlowController::kCategories (6)
                    // categories. Set i takes its label, colour and values from
                    // category i of the controller, so they cannot disagree (#72).
                    BarSeries {
                        barsType: BarSeries.BarsType.Stacked
                        barDelegate: Component { HoverBar {} }

                        axisX: BarCategoryAxis {
                            Component.onCompleted: categories = controller.monthLabels
                        }

                        BarSet { label: page.categoryName(0); color: page.categoryColor(0); Component.onCompleted: values = controller.categoryValues(0) }
                        BarSet { label: page.categoryName(1); color: page.categoryColor(1); Component.onCompleted: values = controller.categoryValues(1) }
                        BarSet { label: page.categoryName(2); color: page.categoryColor(2); Component.onCompleted: values = controller.categoryValues(2) }
                        BarSet { label: page.categoryName(3); color: page.categoryColor(3); Component.onCompleted: values = controller.categoryValues(3) }
                        BarSet { label: page.categoryName(4); color: page.categoryColor(4); Component.onCompleted: values = controller.categoryValues(4) }
                        BarSet { label: page.categoryName(5); color: page.categoryColor(5); Component.onCompleted: values = controller.categoryValues(5) }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
