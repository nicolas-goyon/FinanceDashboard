pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Investments — PROJECT.md page 4 ───────────────────────────────────────────
// Portfolio value vs cumulative deposits · allocation donut · monthly perf bars
// [MOCK DATA — InvestmentsController]

Item {
    InvestmentsController { id: controller }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            PageHeader {
                title: "Investments"
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

            // ── Portfolio value vs deposits ───────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 360
                title: "Portfolio value vs cumulative deposits (€) — the gap is real P&L"

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
                        max: controller.valueMax
                        labelFormat: "%.0f"
                    }

                    LineSeries {
                        name: "Portfolio value"
                        color: Theme.blue
                        width: 2.5
                        Component.onCompleted: {
                            var t = controller.timestamps(); var v = controller.portfolioValues()
                            for (var i = 0; i < t.length; i++) append(t[i], v[i])
                        }
                        pointDelegate: Component {
                            HoverPoint { seriesName: "Portfolio value"; pointColor: Theme.blue }
                        }
                    }
                    LineSeries {
                        name: "Cumulative deposits"
                        color: Theme.overlay0
                        width: 2
                        Component.onCompleted: {
                            var t = controller.timestamps(); var v = controller.depositValues()
                            for (var i = 0; i < t.length; i++) append(t[i], v[i])
                        }
                        pointDelegate: Component {
                            HoverPoint { seriesName: "Cumulative deposits"; pointColor: Theme.overlay0 }
                        }
                    }
                }
            }

            // ── Allocation + monthly performance ──────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ChartCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 320
                    title: "Allocation by asset class — click a slice"

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
                                    allocInfo.text = slice.label + "  —  "
                                        + Math.round(slice.percentage * 100) + " %"
                                }
                            }
                        }
                        Text {
                            id: allocInfo
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
                    title: "Monthly performance %"

                    GraphsView {
                        anchors.fill: parent
                        theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                        axisY: ValueAxis {
                            min: controller.perfMin
                            max: controller.perfMax
                            labelFormat: "%.1f %%"
                        }

                        BarSeries {
                            barDelegate: Component { HoverBar { suffix: " %"; decimals: 1 } }

                            axisX: BarCategoryAxis {
                                Component.onCompleted: categories = controller.monthLabels
                            }
                            BarSet {
                                label: "Gain"
                                color: Theme.green
                                Component.onCompleted: values = controller.perfGains
                            }
                            BarSet {
                                label: "Loss"
                                color: Theme.red
                                Component.onCompleted: values = controller.perfLosses
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
