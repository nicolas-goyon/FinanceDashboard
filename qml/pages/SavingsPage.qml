import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Savings — PROJECT.md page 3 ───────────────────────────────────────────────
// Balance lines per account · ceiling/goal progress bars · yearly interest bars
// [MOCK DATA — SavingsController]

Item {
    SavingsController { id: controller }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            PageHeader {
                title: "Savings"
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

            // ── Balances over time ────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 340
                title: "Balance over time — per account (€)"

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
                        max: controller.balanceMax
                        labelFormat: "%.0f"
                    }

                    LineSeries {
                        name: "Livret A"
                        color: "#89b4fa"
                        width: 2
                        Component.onCompleted: {
                            var t = controller.timestamps(); var v = controller.accountValues(0)
                            for (var i = 0; i < t.length; i++) append(t[i], v[i])
                        }
                        pointDelegate: Component {
                            HoverPoint { seriesName: "Livret A"; pointColor: "#89b4fa" }
                        }
                    }
                    LineSeries {
                        name: "LDDS"
                        color: "#a6e3a1"
                        width: 2
                        Component.onCompleted: {
                            var t = controller.timestamps(); var v = controller.accountValues(1)
                            for (var i = 0; i < t.length; i++) append(t[i], v[i])
                        }
                        pointDelegate: Component {
                            HoverPoint { seriesName: "LDDS"; pointColor: "#a6e3a1" }
                        }
                    }
                    LineSeries {
                        name: "Assurance Vie"
                        color: "#f9e2af"
                        width: 2
                        Component.onCompleted: {
                            var t = controller.timestamps(); var v = controller.accountValues(2)
                            for (var i = 0; i < t.length; i++) append(t[i], v[i])
                        }
                        pointDelegate: Component {
                            HoverPoint { seriesName: "Assurance Vie"; pointColor: "#f9e2af" }
                        }
                    }
                }
            }

            // ── Progress toward ceilings / goals ──────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: goalsCol.implicitHeight + 50
                title: "Progress toward ceilings and goals"

                ColumnLayout {
                    id: goalsCol
                    anchors.fill: parent
                    spacing: 14

                    Repeater {
                        model: controller.goals
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    font.pixelSize: 15
                                    color: "#cdd6f4"
                                }
                                Text {
                                    text: modelData.currentText + "  /  " + modelData.targetText
                                    font.pixelSize: 14
                                    color: "#6c7086"
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 8
                                radius: 4
                                color: "#11111b"
                                Rectangle {
                                    width: parent.width * modelData.pct
                                    height: parent.height
                                    radius: 4
                                    color: modelData.color
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            // ── Interest per year ─────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 320
                title: "Interest / gains earned per year (€)"

                GraphsView {
                    anchors.fill: parent
                    theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                    axisY: ValueAxis {
                        min: 0
                        max: controller.interestMax
                        labelFormat: "%.0f"
                    }

                    BarSeries {
                        barDelegate: Component { HoverBar {} }

                        axisX: BarCategoryAxis {
                            Component.onCompleted: categories = controller.yearLabels
                        }
                        BarSet {
                            label: "Livret A"
                            color: "#89b4fa"
                            Component.onCompleted: values = controller.interestValues(0)
                        }
                        BarSet {
                            label: "LDDS"
                            color: "#a6e3a1"
                            Component.onCompleted: values = controller.interestValues(1)
                        }
                        BarSet {
                            label: "Assurance Vie"
                            color: "#f9e2af"
                            Component.onCompleted: values = controller.interestValues(2)
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }
}
