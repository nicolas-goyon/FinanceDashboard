import QtQuick
import QtQuick.Controls

// barDelegate for BarSeries — replaces the default bar rectangle and adds a
// tooltip on hover. Dynamic properties injected by Qt Graphs (documented
// names): barColor, barBorderColor, barBorderWidth, barValue, barLabel.

Rectangle {
    id: bar

    // Injected by the series
    property color  barColor
    property color  barBorderColor
    property real   barBorderWidth
    property real   barValue
    property string barLabel

    // Configured per chart
    property string suffix: " €"
    property int    decimals: 0

    color: hh.hovered ? Qt.lighter(barColor, 1.2) : barColor
    // The fill colour, not barBorderColor: without a borderColors list on the
    // series, that is the outline colour of the theme on every bar (#71).
    border.color: bar.color
    border.width: barBorderWidth

    HoverHandler { id: hh }

    // A ToolTip is a Popup: it is drawn in the overlay of the window. A plain
    // child Rectangle was drawn under the bars created after this one (each
    // bar is a sibling, and the renderer resets their z), and the plot area
    // clipped it at the top (#59).
    ToolTip {
        visible: hh.hovered && bar.barValue !== 0
        x: (bar.width - width) / 2
        y: -height - 4
        padding: 0
        topPadding: 5
        bottomPadding: 5
        leftPadding: 8
        rightPadding: 8

        background: Rectangle {
            radius: 4
            color: Theme.mantle
            border.color: Theme.surface1
        }

        contentItem: Column {
            spacing: 1

            Text {
                visible: bar.barLabel.length > 0
                text: bar.barLabel
                font.pixelSize: 13
                font.bold: true
                color: bar.barColor
            }
            Text {
                text: bar.barValue.toLocaleString(Qt.locale("fr-FR"), 'f', bar.decimals)
                      + bar.suffix
                font.pixelSize: 15
                font.bold: true
                color: Theme.text
            }
        }
    }
}
