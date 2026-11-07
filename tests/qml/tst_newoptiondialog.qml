import QtQuick
import QtTest
import FinanceDashboard

// NewOptionDialog (#237): the name field has the focus when the dialog opens.
// Add gives the trimmed name and the colour, then closes the dialog. An empty
// name does nothing.
Item {
    id: root
    width: 600
    height: 500

    NewOptionDialog {
        id: dialog
        withColor: true
    }

    SignalSpy { id: acceptedSpy; target: dialog; signalName: "optionAccepted" }

    TestCase {
        name: "NewOptionDialog"
        when: windowShown

        function init() {
            acceptedSpy.clear()
        }

        function cleanup() {
            dialog.close()
            tryVerify(() => !dialog.visible, 2000)
        }

        // The first visible item under `item` with this `text`.
        function findByText(item, text) {
            for (const child of item.children) {
                if (child.visible && child.text === text)
                    return child
                const found = findByText(child, text)
                if (found)
                    return found
            }
            return null
        }

        function openDialog() {
            dialog.openWith("New category", "e.g. Health", "#89b4fa")
            tryVerify(() => dialog.opened, 2000, "the dialog did not open")
        }

        function test_addGivesTheTrimmedNameAndTheColour() {
            openDialog()
            keyClick(Qt.Key_Space)
            for (const c of "Health")
                keyClick(c)
            keyClick(Qt.Key_Space)
            mouseClick(findByText(dialog.contentItem, "Add"))
            compare(acceptedSpy.count, 1)
            compare(acceptedSpy.signalArguments[0][0], "Health")
            compare(acceptedSpy.signalArguments[0][1], "#89b4fa")
            tryVerify(() => !dialog.visible, 2000, "the dialog did not close")
        }

        function test_anEmptyNameDoesNothing() {
            openDialog()
            keyClick(Qt.Key_Space)
            mouseClick(findByText(dialog.contentItem, "Add"))
            compare(acceptedSpy.count, 0)
            verify(dialog.opened)
        }

        function test_noColourWithoutWithColor() {
            dialog.withColor = false
            openDialog()
            keyClick("A")
            mouseClick(findByText(dialog.contentItem, "Add"))
            compare(acceptedSpy.count, 1)
            compare(acceptedSpy.signalArguments[0][0], "A")
            compare(acceptedSpy.signalArguments[0][1], "")
            dialog.withColor = true
        }
    }
}
