import QtQuick
import QtQuick.Controls

// ── TableScrollView ──────────────────────────────────────────────────────────
// A ScrollView for a wide table inside a page that scrolls vertically. The
// table scrolls sideways; the page scrolls up and down.
//
// A ScrollView is a Control with wheelEnabled true, so by default it accepts
// every wheel event over it, also the vertical ones that it cannot use: the
// page under the pointer did not scroll (the user had to move the pointer to
// the edge of the window). Here wheelEnabled is false, so a vertical wheel
// goes on to the page, and the WheelHandler moves the table for a sideways
// wheel (Shift + wheel, touchpad).
ScrollView {
    id: tableScroll

    wheelEnabled: false

    WheelHandler {
        orientation: Qt.Horizontal
        target: null
        // An active handler takes every wheel event, also a vertical one, and
        // a mouse wheel keeps it active for activeTimeout after each event
        // (0.1 s by default): it ends at once here.
        activeTimeout: 0
        onWheel: (event) => {
            const flick = tableScroll.contentItem as Flickable
            if (flick === null)
                return
            const step = event.angleDelta.x !== 0 ? event.angleDelta.x : event.pixelDelta.x
            const maxX = Math.max(0, flick.contentWidth - flick.width)
            flick.contentX = Math.max(0, Math.min(maxX, flick.contentX - step))
        }
    }
}
