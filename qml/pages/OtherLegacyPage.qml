import QtQuick
import QtQuick.Layouts

// ── Other - To remove ──────────────────────────────────────────────────────
// Not a PROJECT.md page. Lists the previous-generation qml/tabs/*.qml files
// that are kept on disk as porting reference (see ARCHITECTURE.MD §
// "Previous generations kept on disk but NOT compiled/registered") but are
// not part of the current product spec. Cleanup candidates once the DB
// wiring phase reuses what it needs from them.

Item {
    id: page

    readonly property var legacyFiles: [
        { name: "AnalyticsTab.qml", note: "Stacked net-worth area chart — reference for Overview wiring" },
        { name: "DatabaseTab.qml", note: "Editable snapshot table — reference for Data Entry history wiring" },
        { name: "EntryTab.qml", note: "Monthly snapshot form — already talks to the real DB" },
        { name: "ImportTab.qml", note: "JSON portfolio import" },
        { name: "OverviewTab.qml", note: "Legacy overview tab (pre-PROJECT.md)" },
        { name: "PortfolioTab.qml", note: "Legacy portfolio tab (pre-PROJECT.md)" },
        { name: "PlaceholderTab.qml", note: "Generic placeholder used while porting" }
    ]

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        Text {
            text: "Other — To remove"
            color: "#cdd6f4"
            font { pixelSize: 23; bold: true }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "These files exist under qml/tabs/ but are not part of PROJECT.md and are not registered in Main.qml. "
                + "They're kept only as a porting reference for the DB-wiring phase. Delete once no longer needed."
            color: "#a6adc8"
            font.pixelSize: 15
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e2e"
            radius: 8
            border.color: "#313244"

            ColumnLayout {
                anchors { fill: parent; margins: 16 }
                spacing: 0

                Repeater {
                    model: page.legacyFiles

                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: modelData.name
                            color: "#f38ba8"
                            font { pixelSize: 16; bold: true; family: "monospace" }
                        }
                        Text {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 12
                            wrapMode: Text.WordWrap
                            text: modelData.note
                            color: "#6c7086"
                            font.pixelSize: 14
                        }
                    }
                }
            }
        }
    }
}
