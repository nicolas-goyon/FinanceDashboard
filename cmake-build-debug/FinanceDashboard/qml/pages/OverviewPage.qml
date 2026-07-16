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

            RowLayout {
                Text {
                    text: "Overview — Net Worth"
                    font.pixelSize: 23
                    font.bold: true
                    color: "#cdd6f4"
                }
                Text { text: "[MOCK]"; font.pixelSize: 14; color: "#f38ba8" }
                Item { Layout.fillWidth: true }
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
                        color: "#6e89b4fa"
                        borderColor: "#89b4fa"
                        borderWidth: 1.5
                        upperSeries: checkingBound
                    }
                    AreaSeries {
                        name: "Savings"
                        color: "#6ea6e3a1"
                        borderColor: "#a6e3a1"
                        borderWidth: 1.5
                        upperSeries: savingsBound
                        lowerSeries: checkingBound
                    }
                    AreaSeries {
                        name: "Trading"
                        color: "#6ecba6f7"
                        borderColor: "#cba6f7"
                        borderWidth: 1.5
                        upperSeries: tradingBound
                        lowerSeries: savingsBound
                    }

                    // ── Invisible hover-carrier lines on each layer boundary ──
                    // (AreaSeries has no pointDelegate; these transparent lines
                    // put hoverable marker dots at the top of each layer and the
                    // tooltip shows the layer's OWN value.)
                    LineSeries {
                        id: hoverChecking
                        color: "transparent"
                        pointDelegate: Component {
                            HoverPoint {
                                seriesName: "Checking"
                                pointColor: "#89b4fa"
                                overrideValues: page.checkingOwn
                            }
                        }
                    }
                    LineSeries {
                        id: hoverSavings
                        color: "transparent"
                        pointDelegate: Component {
                            HoverPoint {
                                seriesName: "Savings"
                                pointColor: "#a6e3a1"
                                overrideValues: page.savingsOwn
                            }
                        }
                    }
                    LineSeries {
                        id: hoverTrading
                        color: "transparent"
                        pointDelegate: Component {
                            HoverPoint {
                                seriesName: "Trading"
                                pointColor: "#cba6f7"
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
                                pieSize: 0.75
                                holeSize: 0.42

                                Component.onCompleted: {
                                    var a = controller.allocation
                                    for (var i = 0; i < a.length; i++) {
                                        var slice = append(a[i].label, a[i].value)
                                        slice.color = a[i].color
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
                            color: "#6c7086"
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
                                color: "#a6e3a1"
                                Component.onCompleted: values = controller.netGains
                            }
                            BarSet {
                                label: "Loss"
                                color: "#f38ba8"
                                Component.onCompleted: values = controller.netLosses
                            }
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }
}
