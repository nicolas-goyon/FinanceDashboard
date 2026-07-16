import QtQuick

// pointDelegate for LineSeries — small marker dot + tooltip on hover.
// Dynamic properties injected by Qt Graphs (declared with the documented
// names): pointColor, pointValueX, pointValueY, pointIndex (6.9+).
//
// Note: € renders fine in QML Text (the axis-label "?" problem is specific
// to the axis label pipeline), so tooltips keep the symbol.

Item {
    id: root

    // Injected by the series
    property color pointColor
    property real  pointValueX
    property real  pointValueY
    property int   pointIndex: -1

    // Configured per chart
    property string seriesName: ""
    property string suffix: " €"
    property int    decimals: 0
    property bool   xIsTime: true      // pointValueX is ms since epoch
    // For stacked charts: the layer's own values (tooltip shows these
    // instead of the cumulative boundary y)
    property var    overrideValues: null

    readonly property real displayValue:
        overrideValues !== null && pointIndex >= 0 && pointIndex < overrideValues.length
            ? overrideValues[pointIndex]
            : pointValueY

    width: 26
    height: 26

    // Always-visible marker dot
    Rectangle {
        anchors.centerIn: parent
        width: hh.hovered ? 11 : 7
        height: width
        radius: width / 2
        color: "#1e1e2e"
        border.color: root.pointColor
        border.width: 2
    }

    HoverHandler { id: hh }

    // Tooltip
    Rectangle {
        visible: hh.hovered
        z: 100
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: 2
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
                visible: root.seriesName.length > 0
                text: root.seriesName
                font.pixelSize: 13
                font.bold: true
                color: root.pointColor
            }
            Text {
                visible: root.xIsTime
                text: Qt.formatDate(new Date(root.pointValueX), "MMM yyyy")
                font.pixelSize: 13
                color: "#6c7086"
            }
            Text {
                text: root.displayValue.toLocaleString(Qt.locale("fr-FR"), 'f', root.decimals)
                      + root.suffix
                font.pixelSize: 15
                font.bold: true
                color: "#cdd6f4"
            }
        }
    }
}
