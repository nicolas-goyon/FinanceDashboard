pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Overview (Net Worth) — PROJECT.md page 1 ──────────────────────────────────
// KPI cards · stacked area by group of accounts · allocation donut · net
// change bars. From the accounts of the net worth (#206, #209):
// NetWorthController reads them again each time the page is shown.
//
// Note: axis labelFormat cannot render "€" (shows "?"), so units live in the
// card titles; tooltips (QML Text) keep the € symbol.

Item {
    id: page

    NetWorthController { id: controller }

    // Stacked-area boundaries (outside GraphsView — never drawn as lines)
    LineSeries { id: checkingBound }
    LineSeries { id: savingsBound }
    LineSeries { id: tradingBound }

    // Per-layer OWN values for the hover tooltips (not cumulative)
    property var checkingOwn: []
    property var savingsOwn: []
    property var tradingOwn: []

    // Fills the series from the controller. Also after a reload: the series
    // are cleared first.
    function fill() {
        checkingBound.clear()
        savingsBound.clear()
        tradingBound.clear()
        hoverChecking.clear()
        hoverSavings.clear()
        hoverTrading.clear()
        var t  = controller.timestamps()
        var b1 = controller.dayToDayBound()
        var b2 = controller.savingsBound()
        var b3 = controller.investmentsBound()
        var own1 = [], own2 = [], own3 = []
        for (var i = 0; i < t.length; i++) {
            checkingBound.append(t[i], b1[i])
            savingsBound.append(t[i], b2[i])
            tradingBound.append(t[i], b3[i])
            hoverChecking.append(t[i], b1[i])
            hoverSavings.append(t[i], b2[i])
            hoverTrading.append(t[i], b3[i])
            own1.push(b1[i])
            own2.push(b2[i] - b1[i])
            own3.push(b3[i] - b2[i])
        }
        checkingOwn = own1
        savingsOwn = own2
        tradingOwn = own3

        allocationSeries.clear()
        var a = controller.allocation
        for (var j = 0; j < a.length; j++) {
            var slice = allocationSeries.append(a[j].label, a[j].value)
            slice.color = a[j].color
            // Without it, the slice gets the outline colour of the theme (#71).
            slice.borderColor = slice.color
            slice.labelVisible = true
        }
        allocationInfo.text = "Click a slice for details"
        monthAxis.categories = controller.monthLabels
        gainSet.values = controller.netGains
        lossSet.values = controller.netLosses
    }

    // Hover-carrier lines are children of the GraphsView: they complete before
    // this handler runs, so they are filled here.
    Component.onCompleted: page.fill()
    // The page stays loaded in the StackLayout: read the data again each time
    // it is shown (an import, a transfer or a valuation can change it).
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
                title: "Overview — Net Worth"
            }

            EmptyState {
                visible: !controller.hasData
                message: "No account is in your net worth yet. Add accounts in Account Management → Manage "
                    + "Accounts, and import their statements."
            }

            // ── KPI cards ─────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Repeater {
                    model: controller.kpis
                    delegate: KpiCard {
                        required property var modelData
                        Layout.fillWidth: true
                        label: modelData.label
                        value: modelData.value
                        sub: modelData.sub
                        valueColor: modelData.color
                    }
                }
            }

            // ── Net worth over time (stacked by account type) ─────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 360
                title: "Net worth over time — day-to-day, savings, investments (€) — hover the dots for values"

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
                        min: controller.minTotal
                        max: controller.maxTotal
                        labelFormat: "%.0f"
                    }

                    AreaSeries {
                        name: "Day-to-day"
                        color: Theme.blueArea
                        borderColor: Theme.blue
                        borderWidth: 1.5
                        upperSeries: checkingBound
                    }
                    AreaSeries {
                        name: "Savings"
                        color: Theme.greenArea
                        borderColor: Theme.green
                        borderWidth: 1.5
                        upperSeries: savingsBound
                        lowerSeries: checkingBound
                    }
                    AreaSeries {
                        name: "Investments"
                        color: Theme.mauveArea
                        borderColor: Theme.mauve
                        borderWidth: 1.5
                        upperSeries: tradingBound
                        lowerSeries: savingsBound
                    }

                    // ── Hover-carrier lines on each layer boundary ──
                    // AreaSeries has no pointDelegate. These lines put hoverable
                    // marker dots at the top of each layer, and the tooltip shows
                    // the layer's own value. Each line has the border colour and
                    // width of its layer, so it covers that border exactly. Do not
                    // set "transparent": Qt Graphs replaces a colour with alpha 0
                    // by the theme colour. The series colour is also the
                    // pointColor of each HoverPoint.
                    LineSeries {
                        id: hoverChecking
                        color: Theme.blue
                        width: 1.5
                        pointDelegate: Component {
                            HoverPoint {
                                seriesName: "Day-to-day"
                                overrideValues: page.checkingOwn
                            }
                        }
                    }
                    LineSeries {
                        id: hoverSavings
                        color: Theme.green
                        width: 1.5
                        pointDelegate: Component {
                            HoverPoint {
                                seriesName: "Savings"
                                overrideValues: page.savingsOwn
                            }
                        }
                    }
                    LineSeries {
                        id: hoverTrading
                        color: Theme.mauve
                        width: 1.5
                        pointDelegate: Component {
                            HoverPoint {
                                seriesName: "Investments"
                                overrideValues: page.tradingOwn
                            }
                        }
                    }
                }
            }

            // ── Allocation + monthly net change ───────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ChartCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 320
                    title: "Current allocation — click a slice for details"

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        GraphsView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                            PieSeries {
                                id: allocationSeries
                                // Without it the renderer emits no clicked signal (default false).
                                selectable: true
                                pieSize: 0.75
                                holeSize: 0.42
                                // Filled by page.fill().
                                onClicked: (slice) => {
                                    allocationInfo.text = slice.label + "  —  "
                                        + slice.value.toLocaleString(Qt.locale("fr-FR"), 'f', 0)
                                        + " €  (" + Math.round(slice.percentage * 100) + " %)"
                                }
                            }
                        }
                        Text {
                            id: allocationInfo
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
                    implicitHeight: 320
                    title: "Change of the net worth each month (€)"

                    GraphsView {
                        anchors.fill: parent
                        theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                        axisY: ValueAxis {
                            min: controller.netChangeMin
                            max: controller.netChangeMax
                            labelFormat: "%.0f"
                        }

                        BarSeries {
                            barDelegate: Component { HoverBar {} }

                            // Filled by page.fill().
                            axisX: BarCategoryAxis { id: monthAxis }
                            BarSet {
                                id: gainSet
                                label: "Gain"
                                color: Theme.green
                            }
                            BarSet {
                                id: lossSet
                                label: "Loss"
                                color: Theme.red
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
