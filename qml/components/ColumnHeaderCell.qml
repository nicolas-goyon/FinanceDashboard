import QtQuick
import QtQuick.Layouts

// ── ColumnHeaderCell — a table header label with a drag-to-resize handle ──────
// Used by ImportSnapshotPage.qml's transaction-review table header (see
// ARCHITECTURE.MD § Statement Import / § Shared Components). Caller sets
// `Layout.preferredWidth` from a shared column-width source (a per-page
// `QtObject`, persisted via `QtCore`'s `Settings` — see ImportSnapshotPage.qml)
// and updates it in `onWidthChangeRequested`; this component never mutates the
// width itself; it only proposes a new one, so the caller stays the single
// source of truth (the same value also drives the width of that column's cells
// in every data row).
//
// The handle overlays the cell's right edge (via `anchors.rightMargin: -3`,
// verified against doc.qt.io/qt-6/qml-qtquick-anchors.html — negative anchor
// margins are documented/supported) rather than being inserted as a sibling
// `RowLayout` item, so it never changes this header row's total preferred
// width. That matters because the data rows below (`ImportSnapshotPage.qml`'s
// `Repeater` delegate) use the exact same `RowLayout` spacing with no handle
// items of their own — adding a handle as a real layout sibling here would
// throw the header out of alignment with every column below it.
Item {
    id: cell

    property alias text: label.text
    property int horizontalAlignment: Text.AlignLeft
    property real minWidth: 40
    property real maxWidth: 500

    signal widthChangeRequested(real newWidth)

    implicitHeight: label.implicitHeight
    Layout.fillHeight: true // fill the header row's height so the drag
                             // handle spans it fully, not just the text's
                             // own implicit height — overridable per
                             // instance same as any other Layout.* default.

    Text {
        id: label
        anchors.fill: parent
        font.pixelSize: 13
        font.bold: true
        color: Theme.overlay0
        horizontalAlignment: cell.horizontalAlignment
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    // Thin drag handle straddling the cell's right edge — 2px visible grip,
    // 6px hit target (MouseArea) for a forgiving drag target without making
    // the visible line intrusive at rest.
    Item {
        id: handle
        width: 6
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.rightMargin: -3
        z: 10

        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: parent.height * 0.6
            radius: 1
            color: dragArea.containsMouse || dragArea.pressed ? Theme.blue : "transparent"
        }

        MouseArea {
            id: dragArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            // The table is in a horizontal ScrollView (#60): without this, its
            // Flickable takes the drag after a few pixels.
            preventStealing: true
            property real lastX: 0

            onPressed: (mouse) => { lastX = mouse.x }
            onPositionChanged: (mouse) => {
                if (!pressed)
                    return
                var delta = mouse.x - lastX
                if (delta !== 0) {
                    var proposed = Math.max(cell.minWidth, Math.min(cell.maxWidth, cell.width + delta))
                    cell.widthChangeRequested(proposed)
                }
            }
        }
    }
}
