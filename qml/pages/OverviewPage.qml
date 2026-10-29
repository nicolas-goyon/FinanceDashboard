pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Overview (Net Worth) — PROJECT.md page 1 ──────────────────────────────────
// KPI cards · stacked area by account type · allocation donut · net change bars
// [MOCK DATA — NetWorthController]
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

    Component.onCompleted: {
        var t  = controller.timestamps()
        var b1 = controller.checkingBound()
        var b2 = controller.savingsBound()
        var b3 = controller.tradingBound()
        var own1 = [], own2 = [], own3 = []
        for (var i = 0; i < t.length; i++) {
            checkingBound.append(t[i], b1[i])
            savingsBound.append(t[i], b2[i])
            tradingBound.append(t[i], b3[i])
            // Hover-carrier lines (children complete before this handler runs,
            // so they must be filled here, not in their own onCompleted)
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
                mock: true
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
                title: "Net worth over time — by account type (€) — hover the dots for values"

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
                        max: controller.maxTotal
                        labelFormat: "%.0f"
                    }

                    AreaSeries {
                        name: "Checking"
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
                        name: "Trading"
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
                                seriesName: "Checking"
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
                                seriesName: "Trading"
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
                                // Without it the renderer emits no clicked signal (default false).
                                selectable: true
                                pieSize: 0.75
                                holeSize: 0.42

                                Component.onCompleted: {
                                    var a = controller.allocation
                                    for (var i = 0; i < a.length; i++) {
                                        var slice = append(a[i].label, a[i].value)
                                        slice.color = a[i].color
                                        // Without it, the slice gets the outline colour of the theme (#71).
                                        slice.borderColor = slice.color
                                        slice.labelVisible = true
                                    }
                                }
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
                    title: "Monthly net change (€)"

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

                            axisX: BarCategoryAxis {
                                Component.onCompleted: categories = controller.monthLabels
                            }
                            BarSet {
                                label: "Gain"
                                color: Theme.green
                                Component.onCompleted: values = controller.netGains
                            }
                            BarSet {
                                label: "Loss"
                                color: Theme.red
                                Component.onCompleted: values = controller.netLosses
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
