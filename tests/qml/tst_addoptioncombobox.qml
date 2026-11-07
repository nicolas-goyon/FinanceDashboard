import QtQuick
import QtTest
import FinanceDashboard

// AddOptionComboBox and ColorComboBox (#237): the list has a last row
// "＋ New…" when `addText` is set. A click on it closes the list and emits
// `addRequested()`, with no change to the selection and no `activated`. The
// row is not an item of the model, and the list also opens when the model is
// empty (the target account of the import review when there is no account).
Item {
    id: root
    width: 600
    height: 500

    property var names: ["Checking", "Savings"]

    Column {
        AddOptionComboBox {
            id: box
            width: 220
            model: root.names
            addText: "＋ New type…"
        }
        AddOptionComboBox {
            id: emptyBox
            width: 220
            model: []
            addText: "＋ New account…"
        }
        ColorComboBox {
            id: colorBox
            width: 220
            items: [{ name: "Food", color: "#a6e3a1" }, { name: "Rent", color: "#f38ba8" }]
            currentName: "Rent"
            addText: "＋ New category…"
        }
        ColorComboBox {
            id: plainColorBox
            width: 220
            items: [{ name: "Food", color: "#a6e3a1" }]
        }
    }

    SignalSpy { id: addSpy; signalName: "addRequested" }
    SignalSpy { id: activatedSpy; signalName: "activated" }

    TestCase {
        name: "AddOptionComboBox"
        when: windowShown

        function init() {
            addSpy.clear()
            activatedSpy.clear()
        }

        function cleanup() {
            for (const b of [box, emptyBox, colorBox, plainColorBox])
                b.popup.close()
            wait(50)
        }

        function openList(combo) {
            mouseClick(combo)
            tryVerify(() => combo.popup.opened, 2000, "the list did not open")
            const list = combo.popup.contentItem
            tryVerify(() => list.footerItem === null || list.footerItem.height > 0, 2000)
            return list
        }

        function clickLastRow(combo) {
            const row = combo.popup.contentItem.footerItem
            verify(row !== null, "no last row")
            mouseClick(row)
            tryVerify(() => !combo.popup.visible, 2000, "the list did not close")
        }

        function test_lastRowAddsWithoutChangingTheSelection() {
            addSpy.target = box
            activatedSpy.target = box
            box.currentIndex = 1
            const list = openList(box)
            compare(list.footerItem.text, "＋ New type…")
            compare(box.count, 2)
            clickLastRow(box)
            compare(addSpy.count, 1)
            compare(activatedSpy.count, 0)
            compare(box.currentIndex, 1)
            compare(box.count, 2)
        }

        function test_emptyListOpensWithTheLastRow() {
            addSpy.target = emptyBox
            openList(emptyBox)
            compare(emptyBox.count, 0)
            clickLastRow(emptyBox)
            compare(addSpy.count, 1)
        }

        function test_colorComboBoxHasTheLastRow() {
            addSpy.target = colorBox
            activatedSpy.target = colorBox
            const list = openList(colorBox)
            compare(list.footerItem.text, "＋ New category…")
            clickLastRow(colorBox)
            compare(addSpy.count, 1)
            compare(activatedSpy.count, 0)
            compare(colorBox.currentIndex, 1)
        }

        function test_noLastRowWithoutAddText() {
            const list = openList(plainColorBox)
            compare(list.footerItem, null)
        }

        function test_anItemOfTheListStillActivates() {
            activatedSpy.target = box
            box.currentIndex = 1
            const list = openList(box)
            tryVerify(() => list.itemAtIndex(0) !== null, 2000)
            mouseClick(list.itemAtIndex(0))
            compare(activatedSpy.count, 1)
            compare(box.currentIndex, 0)
        }
    }
}
