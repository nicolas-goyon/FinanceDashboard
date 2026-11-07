import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtTest
import "../../qml/components"

// TableScrollView: a vertical wheel over a wide table scrolls the page under
// it; a sideways wheel moves the table. Before, the ScrollView of the table
// took every wheel event (the import review, the transactions page, the
// account history).
Item {
    id: root
    width: 600
    height: 400

    ScrollView {
        id: page
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: page.availableWidth
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 100; color: "gray" }
            // The set-up of the table of ImportSnapshotPage.
            TableScrollView {
                id: tableScroll
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: table.implicitHeight
                    + (tableScroll.ScrollBar.horizontal.visible ? tableScroll.ScrollBar.horizontal.height : 0)
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOn
                contentWidth: table.width
                contentHeight: table.implicitHeight

                ColumnLayout {
                    id: table
                    width: 1500
                    spacing: 0
                    Repeater {
                        model: 10
                        Rectangle {
                            required property int index
                            Layout.preferredWidth: 1500
                            Layout.preferredHeight: 30
                            color: index % 2 ? "white" : "black"
                        }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 900; color: "gray" }
        }
    }

    TestCase {
        name: "TableScrollView"
        when: windowShown

        function init() {
            page.contentItem.contentY = 0
            tableScroll.contentItem.contentX = 0
            wait(50)
        }

        function test_verticalWheelOverTheTableScrollsThePage() {
            const p = tableScroll.mapToItem(root, 200, 100)
            for (let i = 0; i < 5; i++) {
                mouseWheel(root, p.x, p.y, 0, -120)
                wait(30)
            }
            tryVerify(() => page.contentItem.contentY > 150, 2000, "the page did not scroll")
            compare(tableScroll.contentItem.contentY, 0)
        }

        function test_sidewaysWheelMovesTheTable() {
            const p = tableScroll.mapToItem(root, 200, 100)
            mouseWheel(root, p.x, p.y, -120, 0)
            tryVerify(() => tableScroll.contentItem.contentX > 0, 2000, "the table did not move")
            compare(page.contentItem.contentY, 0)
        }
    }
}
