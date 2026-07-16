import QtQuick

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
    border.color: barBorderColor
    border.width: barBorderWidth

    HoverHandler { id: hh }

    Rectangle {
        visible: hh.hovered && bar.barValue !== 0
        z: 100
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: 4
        width: tipCol.implicitWidth + 16
        height: tipCol.implicitHeight + 10
        radius: 4
        color: "#181825"
        border.color: "#45475a"

        Column {
            id: tipCol
            anchors.centerIn: parent
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
                color: "#cdd6f4"
            }
        }
    }
}
