import QtQuick
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Analytics tab ─────────────────────────────────────────────────────────────
// Stacked area chart of portfolio value over time, one layer per asset
// category (bottom → top): ETFs, Metals, Coins, Stocks.
//
// Pattern follows QtGraphsExamples → areachart/stacked:
// boundary LineSeries are defined OUTSIDE GraphsView (so they are never drawn
// as lines), and each AreaSeries references them as upperSeries/lowerSeries.
// All stacking math is done in C++ (AnalyticsController).

Item {
    id: root

    AnalyticsController { id: controller }

    // ── Boundary LineSeries — cumulative sums, computed in C++ ───────────────
    LineSeries { id: etfsBound   }   // etfs
    LineSeries { id: metalsBound }   // etfs + metals
    LineSeries { id: coinsBound  }   // etfs + metals + coins
    LineSeries { id: stocksBound }   // total

    function loadSeries() {
        etfsBound.clear()
        metalsBound.clear()
        coinsBound.clear()
        stocksBound.clear()
        var t  = controller.timestamps()
        var b1 = controller.etfsBound()
        var b2 = controller.metalsBound()
        var b3 = controller.coinsBound()
        var b4 = controller.stocksBound()
        for (var i = 0; i < t.length; i++) {
            etfsBound.append(t[i], b1[i])
            metalsBound.append(t[i], b2[i])
            coinsBound.append(t[i], b3[i])
            stocksBound.append(t[i], b4[i])
        }
    }

    // Called from Main.qml after a save in the Entry or Database tab.
    function refresh() {
        controller.reload()
        loadSeries()
    }

    Component.onCompleted: loadSeries()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Text {
            text: "Analytics"
            font { pixelSize: 23; bold: true }
            color: "#cdd6f4"
        }

        // ── Empty state ───────────────────────────────────────────────────────
        Rectangle {
            visible: !controller.hasData
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e2e"
            border.color: "#313244"
            radius: 8

            Text {
                anchors.centerIn: parent
                text: "No snapshots recorded yet.\nSave entries in the Entry tab to see the chart."
                horizontalAlignment: Text.AlignHCenter
                color: "#6c7086"
                font.pixelSize: 16
            }
        }

        // ── Chart ─────────────────────────────────────────────────────────────
        GraphsView {
            visible: controller.hasData
            Layout.fillWidth: true
            Layout.fillHeight: true

            theme: GraphsTheme {
                colorScheme: GraphsTheme.ColorScheme.Dark
                plotAreaBackgroundVisible: true
            }

            axisX: DateTimeAxis {
                min: new Date(controller.minTimeMs)
                max: new Date(controller.maxTimeMs)
                labelFormat: "d MMM yy"
            }
            axisY: ValueAxis {
                min: 0
                max: controller.maxTotal
                labelFormat: "%.0f €"
            }

            // Bottom layer — fills from graph bottom (lowerSeries omitted)
            AreaSeries {
                name: "ETFs"
                color: "#6ea6e3a1"       // green ~43 % opacity
                borderColor: "#a6e3a1"
                borderWidth: 1.5
                upperSeries: etfsBound
            }
            AreaSeries {
                name: "Metals"
                color: "#6ef9e2af"       // yellow
                borderColor: "#f9e2af"
                borderWidth: 1.5
                upperSeries: metalsBound
                lowerSeries: etfsBound
            }
            AreaSeries {
                name: "Coins"
                color: "#6ef38ba8"       // pink
                borderColor: "#f38ba8"
                borderWidth: 1.5
                upperSeries: coinsBound
                lowerSeries: metalsBound
            }
            AreaSeries {
                name: "Stocks"
                color: "#6e89b4fa"       // blue
                borderColor: "#89b4fa"
                borderWidth: 1.5
                upperSeries: stocksBound
                lowerSeries: coinsBound
            }
        }
    }
}
