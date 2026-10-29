import QtQuick

// Sankey / money-flow diagram — pure-QML Canvas implementation, adapted from
// QtGraphsExamples → flowchart/basic (Qt Graphs has no Sankey series type).
// inputs → centre node → outputs; both sides must sum to the same total.
//
// inputs / outputs: [{ name, value, color }]

Item {
    id: root

    property var inputs: []
    property var outputs: []
    property string centreName: "Budget"

    function sum(arr) {
        var t = 0
        for (var i = 0; i < arr.length; i++) t += arr[i].value
        return t
    }
    function repaint() { canvas.requestPaint() }

    onInputsChanged: canvas.requestPaint()
    onOutputsChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()

        // Filled bezier band between a source edge segment and a target edge segment
        function ribbon(ctx, x0, y0a, y0b, x1, y1a, y1b, fill) {
            var xc = (x0 + x1) / 2
            ctx.beginPath()
            ctx.moveTo(x0, y0a)
            ctx.bezierCurveTo(xc, y0a, xc, y1a, x1, y1a)
            ctx.lineTo(x1, y1b)
            ctx.bezierCurveTo(xc, y1b, xc, y0b, x0, y0b)
            ctx.closePath()
            ctx.fillStyle = fill
            ctx.fill()
        }

        function withAlpha(hex, a) {
            var r = parseInt(hex.substr(1, 2), 16)
            var g = parseInt(hex.substr(3, 2), 16)
            var b = parseInt(hex.substr(5, 2), 16)
            return "rgba(" + r + "," + g + "," + b + "," + a + ")"
        }

        function euro(v) {
            return v.toLocaleString(Qt.locale("fr-FR"), 'f', 0) + " €"
        }

        onPaint: {
            var ctx = getContext("2d")
            var W = width, H = height
            ctx.reset()
            ctx.clearRect(0, 0, W, H)

            var inp = root.inputs, out = root.outputs
            var n = inp.length, m = out.length
            if (n === 0 || m === 0) return
            var total = root.sum(inp)
            if (total <= 0) return

            // Layout constants
            var marginTop = 28, marginBottom = 28
            var leftLabelW = 110, rightLabelW = 140
            var nodeW = 16, labelGap = 8
            var gap = 14

            var plotH = H - marginTop - marginBottom
            var maxGaps = Math.max(n - 1, m - 1) * gap
            var scale = (plotH - maxGaps) / total

            var xLeft   = leftLabelW
            var xCentre = (W - nodeW) / 2
            var xRight  = W - rightLabelW - nodeW

            var leftStackH  = total * scale + (n - 1) * gap
            var rightStackH = total * scale + (m - 1) * gap
            var centreH     = total * scale

            var leftY0   = marginTop + (plotH - leftStackH) / 2
            var rightY0  = marginTop + (plotH - rightStackH) / 2
            var centreY0 = marginTop + (plotH - centreH) / 2

            ctx.font = "12px sans-serif"
            ctx.textBaseline = "middle"

            // Ribbons: inputs → centre
            var ly = leftY0, cy = centreY0
            for (var i = 0; i < n; i++) {
                var h = inp[i].value * scale
                ribbon(ctx, xLeft + nodeW, ly, ly + h,
                            xCentre,       cy, cy + h,
                            withAlpha(inp[i].color, 0.45))
                ly += h + gap
                cy += h
            }

            // Ribbons: centre → outputs
            var cyR = centreY0, ry = rightY0
            for (var j = 0; j < m; j++) {
                var ho = out[j].value * scale
                ribbon(ctx, xCentre + nodeW, cyR, cyR + ho,
                            xRight,          ry,  ry + ho,
                            withAlpha(out[j].color, 0.45))
                cyR += ho
                ry += ho + gap
            }

            // Nodes + labels: inputs
            ly = leftY0
            ctx.textAlign = "right"
            for (i = 0; i < n; i++) {
                var hi = inp[i].value * scale
                ctx.fillStyle = inp[i].color
                ctx.fillRect(xLeft, ly, nodeW, hi)
                ctx.fillStyle = Theme.text
                ctx.fillText(inp[i].name, xLeft - labelGap, ly + hi / 2 - 7)
                ctx.fillStyle = Theme.overlay0
                ctx.fillText(euro(inp[i].value), xLeft - labelGap, ly + hi / 2 + 8)
                ly += hi + gap
            }

            // Nodes + labels: outputs
            ry = rightY0
            ctx.textAlign = "left"
            for (j = 0; j < m; j++) {
                var hj = out[j].value * scale
                ctx.fillStyle = out[j].color
                ctx.fillRect(xRight, ry, nodeW, hj)
                ctx.fillStyle = Theme.text
                ctx.fillText(out[j].name, xRight + nodeW + labelGap, ry + hj / 2 - 7)
                ctx.fillStyle = Theme.overlay0
                ctx.fillText(euro(out[j].value), xRight + nodeW + labelGap, ry + hj / 2 + 8)
                ry += hj + gap
            }

            // Centre node + label
            ctx.fillStyle = Theme.surface1
            ctx.fillRect(xCentre, centreY0, nodeW, centreH)
            ctx.fillStyle = Theme.text
            ctx.textAlign = "center"
            ctx.fillText(root.centreName, xCentre + nodeW / 2, centreY0 - 12)
            ctx.fillStyle = Theme.overlay0
            ctx.fillText(euro(total), xCentre + nodeW / 2, centreY0 + centreH + 12)
        }
    }
}
