pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Savings — PROJECT.md page 3 ───────────────────────────────────────────────
// Balance lines per account · ceiling progress bars · yearly interest bars.
// From the savings accounts of the net worth (#211).

Item {
    id: page

    SavingsController { id: controller }

    // The name and colour of slot i (controller.seriesSlots). An unused slot
    // has an empty name: its line and its bars are hidden.
    function slotName(i) {
        return i < controller.seriesSlots.length ? controller.seriesSlots[i].name : ""
    }
    function slotColor(i) {
        return i < controller.seriesSlots.length ? controller.seriesSlots[i].color : Theme.overlay0
    }

    function fill() {
        var lines = [line0, line1, line2, line3]
        var t = controller.timestamps()
        for (var k = 0; k < lines.length; k++) {
            lines[k].clear()
            var v = controller.accountValues(k)
            for (var i = 0; i < v.length; i++)
                lines[k].append(t[i], v[i])
        }
        yearAxis.categories = controller.yearLabels
        var sets = [interest0, interest1, interest2, interest3]
        for (var j = 0; j < sets.length; j++)
            sets[j].values = controller.interestValues(j)
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
                title: qsTr("Savings")
            }

            EmptyState {
                visible: !controller.hasData
                message: qsTr("No savings account in your net worth yet. Give an account the category \"Savings\" "
                    + "(or a type such as Livret A or LDDS) in Account Management → Manage Accounts.")
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
                title: qsTr("Balance over time — per account (€)")

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

                    // Four static lines (the four accounts with the highest
                    // balance); filled by page.fill().
                    LineSeries {
                        id: line0
                        name: page.slotName(0)
                        visible: name.length > 0
                        color: page.slotColor(0)
                        width: 2
                        pointDelegate: Component { HoverPoint { seriesName: page.slotName(0); pointColor: page.slotColor(0) } }
                    }
                    LineSeries {
                        id: line1
                        name: page.slotName(1)
                        visible: name.length > 0
                        color: page.slotColor(1)
                        width: 2
                        pointDelegate: Component { HoverPoint { seriesName: page.slotName(1); pointColor: page.slotColor(1) } }
                    }
                    LineSeries {
                        id: line2
                        name: page.slotName(2)
                        visible: name.length > 0
                        color: page.slotColor(2)
                        width: 2
                        pointDelegate: Component { HoverPoint { seriesName: page.slotName(2); pointColor: page.slotColor(2) } }
                    }
                    LineSeries {
                        id: line3
                        name: page.slotName(3)
                        visible: name.length > 0
                        color: page.slotColor(3)
                        width: 2
                        pointDelegate: Component { HoverPoint { seriesName: page.slotName(3); pointColor: page.slotColor(3) } }
                    }
                }
            }

            // ── Progress toward ceilings / goals ──────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: goalsCol.implicitHeight + 50
                title: qsTr("Progress toward the ceilings")

                ColumnLayout {
                    id: goalsCol
                    anchors.fill: parent
                    spacing: 14

                    EmptyState {
                        visible: controller.goals.length === 0
                        message: qsTr("No regulated savings account (Livret A, LDDS, LEP, PEL, CEL, Livret Jeune).")
                    }

                    Repeater {
                        model: controller.goals
                        delegate: ColumnLayout {
                            id: goalRow
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: goalRow.modelData.label
                                    font.pixelSize: 15
                                    color: Theme.text
                                }
                                Text {
                                    text: goalRow.modelData.currentText + "  /  " + goalRow.modelData.targetText
                                    font.pixelSize: 14
                                    color: Theme.overlay0
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 8
                                radius: 4
                                color: Theme.crust
                                Rectangle {
                                    width: parent.width * goalRow.modelData.pct
                                    height: parent.height
                                    radius: 4
                                    color: goalRow.modelData.color
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
                title: qsTr("Interest earned per year (€)")

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

                        // Filled by page.fill().
                        axisX: BarCategoryAxis { id: yearAxis }
                        BarSet { id: interest0; label: page.slotName(0); color: page.slotColor(0) }
                        BarSet { id: interest1; label: page.slotName(1); color: page.slotColor(1) }
                        BarSet { id: interest2; label: page.slotName(2); color: page.slotColor(2) }
                        BarSet { id: interest3; label: page.slotName(3); color: page.slotColor(3) }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
