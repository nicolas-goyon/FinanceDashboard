import QtQuick
import QtQuick.Controls

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
    // Format of the date line (Qt date format). Monthly data shows the month;
    // a chart of daily data sets a format with the day (#73).
    property string dateFormat: "MMM yyyy"
    // For stacked charts: the layer's own values (tooltip shows these
    // instead of the cumulative boundary y)
    property var    overrideValues: null

    readonly property real displayValue:
        overrideValues !== null && pointIndex >= 0 && pointIndex < overrideValues.length
            ? overrideValues[pointIndex]
            : pointValueY

    // Hit-box is deliberately bigger than the visible dot (below) — a 26x26
    // target was reported hard to actually land the cursor on for hover
    // (product feedback on AccountHistoryPage.qml's balance chart); the
    // HoverHandler below reacts anywhere in this larger Item, not just over
    // the visible marker itself.
    width: 34
    height: 34

    // Always-visible marker dot
    Rectangle {
        anchors.centerIn: parent
        width: hh.hovered ? 13 : 8
        height: width
        radius: width / 2
        color: "#1e1e2e"
        border.color: root.pointColor
        border.width: 2
    }

    HoverHandler { id: hh }

    // A ToolTip is a Popup: it is drawn in the overlay of the window. A child
    // Rectangle was drawn under the markers of the points and series created
    // after this one: each point delegate is a sibling, and the renderer sets
    // their z on each update (PointRenderer, setZ(series->zValue())), so a z
    // on the delegate does not stay (#74, as #59 for the bars). The popup
    // stays inside the window, so it is not cut at the edges of the chart.
    ToolTip {
        id: tooltip
        visible: hh.hovered
        x: (root.width - width) / 2
        y: -height - 2
        padding: 0
        topPadding: 5
        bottomPadding: 5
        leftPadding: 8
        rightPadding: 8

        background: Rectangle {
            radius: 4
            color: "#181825"
            border.color: "#45475a"
        }

        contentItem: Column {
            id: tipCol
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
                text: Qt.formatDate(new Date(root.pointValueX), root.dateFormat)
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
