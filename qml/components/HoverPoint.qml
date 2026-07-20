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

    // Tooltip — positioned with plain x/y (not anchors.bottom/horizontalCenter)
    // so it can flip below the point and clamp horizontally instead of
    // always centering above, which cropped it against the chart's edges for
    // any point near the top or a side (product feedback). `root.parent` is
    // Qt Graphs' own delegate-host item, sized to the plot area, so its
    // width/height are the actual bounds to stay inside.
    Rectangle {
        id: tooltip
        visible: hh.hovered
        z: 1000
        width: tipCol.implicitWidth + 16
        height: tipCol.implicitHeight + 10
        radius: 4
        color: "#181825"
        border.color: "#45475a"

        // Flip below the point when there isn't room above within the chart.
        readonly property bool showBelow: root.y < tooltip.height + 6
        y: tooltip.showBelow ? (root.height + 2) : -(tooltip.height + 2)

        // Centered on the point by default, clamped so it never extends past
        // root.parent's left/right edges (root.x/root.parent.width are in
        // root.parent's coordinate space; tooltip.x is relative to root, so
        // the clamp bounds are offset by -root.x).
        readonly property real centeredX: (root.width - tooltip.width) / 2
        x: {
            // Fall back to plain centering if root.parent isn't a
            // sensible/sized item to clamp against (defensive — Qt Graphs'
            // exact delegate-host item isn't documented).
            if (root.parent === null || root.parent.width <= 0) return tooltip.centeredX
            var minX = -root.x
            var maxX = root.parent.width - root.x - tooltip.width
            if (maxX < minX) return tooltip.centeredX // parent narrower than the tooltip itself
            return Math.max(minX, Math.min(tooltip.centeredX, maxX))
        }

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
