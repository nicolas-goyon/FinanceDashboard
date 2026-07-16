import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Entry tab ─────────────────────────────────────────────────────────────────
// Manual snapshot entry: date picker + 4 category cards + save/clear footer.
// All logic (parsing, diffs, totals, DB access) lives in EntryController.

Item {
    id: root

    signal saved()

    EntryController {
        id: controller
        onSnapshotSaved: root.saved()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Text {
            text: "New Entry"
            font { pixelSize: 23; bold: true }
            color: "#cdd6f4"
        }
        Text {
            text: "Record current values for each asset category."
            font.pixelSize: 15
            color: "#6c7086"
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── Date row ──────────────────────────────────────────────────────────
        RowLayout {
            spacing: 10

            Text { text: "Date:"; font.pixelSize: 15; color: "#6c7086" }

            Rectangle {
                id: dateButton
                width: 160; height: 30
                radius: 4
                color: "#1e1e2e"
                border.color: calPopup.visible ? "#89b4fa" : "#313244"

                Text {
                    anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                    text: controller.dateText
                    font.pixelSize: 15
                    color: "#cdd6f4"
                }
                Text {
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    text: "▾"
                    font.pixelSize: 13
                    color: "#6c7086"
                }
                TapHandler {
                    onTapped: calPopup.visible ? calPopup.close() : calPopup.open()
                }

                // ── Calendar popup ────────────────────────────────────────────
                Popup {
                    id: calPopup
                    y: dateButton.height + 4
                    width: 260
                    padding: 10

                    property int viewYear: controller.calYear
                    property int viewMonth: controller.calMonth
                    readonly property date now: new Date()

                    onOpened: {
                        viewYear = controller.calYear
                        viewMonth = controller.calMonth
                    }

                    background: Rectangle {
                        color: "#181825"
                        border.color: "#313244"
                        radius: 6
                    }

                    contentItem: ColumnLayout {
                        spacing: 6

                        // Month navigation
                        RowLayout {
                            Layout.fillWidth: true
                            Rectangle {
                                width: 26; height: 26; radius: 4
                                color: prevHover.hovered ? "#313244" : "transparent"
                                Text { anchors.centerIn: parent; text: "‹"; color: "#89b4fa"; font.pixelSize: 17 }
                                HoverHandler { id: prevHover }
                                TapHandler {
                                    onTapped: {
                                        if (calPopup.viewMonth === 0) {
                                            calPopup.viewMonth = 11; calPopup.viewYear--
                                        } else calPopup.viewMonth--
                                    }
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: Qt.locale().monthName(calPopup.viewMonth) + " " + calPopup.viewYear
                                color: "#cdd6f4"
                                font { pixelSize: 15; bold: true }
                            }
                            Rectangle {
                                width: 26; height: 26; radius: 4
                                color: nextHover.hovered ? "#313244" : "transparent"
                                Text { anchors.centerIn: parent; text: "›"; color: "#89b4fa"; font.pixelSize: 17 }
                                HoverHandler { id: nextHover }
                                TapHandler {
                                    onTapped: {
                                        if (calPopup.viewMonth === 11) {
                                            calPopup.viewMonth = 0; calPopup.viewYear++
                                        } else calPopup.viewMonth++
                                    }
                                }
                            }
                        }

                        DayOfWeekRow {
                            Layout.fillWidth: true
                            locale: grid.locale
                            delegate: Text {
                                required property var model
                                text: model.shortName
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 13
                                color: "#6c7086"
                            }
                        }

                        MonthGrid {
                            id: grid
                            Layout.fillWidth: true
                            month: calPopup.viewMonth
                            year: calPopup.viewYear

                            delegate: Rectangle {
                                required property var model
                                readonly property bool inMonth: model.month === grid.month
                                readonly property bool isFuture: model.date > calPopup.now
                                readonly property bool isSelected:
                                    model.day === controller.calDay
                                    && model.month === controller.calMonth
                                    && model.year === controller.calYear

                                width: 30; height: 26; radius: 4
                                color: isSelected ? "#1e3a5f"
                                     : dayHover.hovered && !isFuture && inMonth ? "#313244"
                                     : "transparent"

                                Text {
                                    anchors.centerIn: parent
                                    text: model.day
                                    font { pixelSize: 14; bold: model.today || isSelected }
                                    color: isFuture ? "#45475a"
                                         : !inMonth ? "#45475a"
                                         : model.today ? "#89b4fa"
                                         : "#cdd6f4"
                                }
                                HoverHandler { id: dayHover }
                            }

                            onClicked: (date) => {
                                if (date > calPopup.now)
                                    return
                                controller.setDateYmd(date.getFullYear(),
                                                      date.getMonth(),
                                                      date.getDate())
                                calPopup.close()
                            }
                        }
                    }
                }
            }

            Text {
                text: "defaults to today  —  older dates allowed"
                font.pixelSize: 14
                color: "#6c7086"
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── Category sections ─────────────────────────────────────────────────
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 14

                EntryCategory { Layout.fillWidth: true; title: "Stocks"; assetType: "Stock"; categoryModel: controller.stocks }
                EntryCategory { Layout.fillWidth: true; title: "Coins";  assetType: "Coin";  categoryModel: controller.coins  }
                EntryCategory { Layout.fillWidth: true; title: "Metals"; assetType: "Metal"; categoryModel: controller.metals }
                EntryCategory { Layout.fillWidth: true; title: "ETFs";   assetType: "ETF";   categoryModel: controller.etfs   }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── Footer ────────────────────────────────────────────────────────────
        RowLayout {
            spacing: 10

            Text {
                text: "Grand Total:  " + controller.grandTotalText
                font { pixelSize: 17; bold: true }
                color: "#cdd6f4"
            }
            Item { Layout.fillWidth: true }
            Text {
                text: controller.statusText
                font.pixelSize: 14
                color: controller.statusIsError ? "#f38ba8" : "#a6e3a1"
            }

            Rectangle {
                width: 90; height: 30; radius: 4
                color: clearHover.hovered ? "#313244" : "#1e1e2e"
                border.color: "#313244"
                Text { anchors.centerIn: parent; text: "Clear all"; font.pixelSize: 15; color: "#cdd6f4" }
                HoverHandler { id: clearHover }
                TapHandler { onTapped: controller.clearAll() }
            }
            Rectangle {
                width: 90; height: 30; radius: 4
                color: saveHover.hovered ? "#264d80" : "#1e3a5f"
                border.color: "#89b4fa"
                Text { anchors.centerIn: parent; text: "Save"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                HoverHandler { id: saveHover }
                TapHandler { onTapped: controller.save() }
            }
        }
    }
}
