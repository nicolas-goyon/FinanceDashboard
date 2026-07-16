import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Data Entry / Reports — PROJECT.md page 6 ──────────────────────────────────
// Monthly snapshot per account · transaction entry with manual category/
// subcategory picker (CategoryController — see Data Entry → "Manage
// Categories" / CategoriesManagePage.qml) · history · annual summary.
// Accounts come from AccountController (QML_SINGLETON — see ARCHITECTURE.MD
// § Account Management); add/edit/remove account UI moved to
// AccountsManagePage.qml (Data Entry → "Manage Accounts") so it isn't
// duplicated here — this page only lists accounts + lets you key in each
// one's balance for the snapshot.
// [MOCK — DataEntryController; the snapshot/transaction forms are not
// persisted yet, only the accounts list itself is real]

Item {
    id: page

    DataEntryController { id: controller }

    // Badge color now comes from the account's *category* (Day-to-day,
    // Savings, Investments, Urgent savings, …) via
    // AccountController.accountCategoryColor() — replaces the old fixed
    // Checking/Savings/Trading type→color map, which no longer matches the
    // real (DB-managed, open-ended) category list.
    function categoryBadgeColor(categoryName) {
        var hex = AccountController.accountCategoryColor(categoryName)
        return hex.length > 0 ? hex : "#6c7086"
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            // ── Header ────────────────────────────────────────────────────────
            RowLayout {
                spacing: 12
                Text {
                    text: "Data Entry / Reports"
                    font.pixelSize: 23
                    font.bold: true
                    color: "#cdd6f4"
                }
                Text { text: "[MOCK]"; font.pixelSize: 14; color: "#f38ba8" }
                Item { Layout.fillWidth: true }
                Rectangle {
                    visible: controller.statusText.length > 0
                    width: statusText.implicitWidth + 20
                    height: 26
                    radius: 13
                    color: "#1a3328"
                    border.color: "#2d5a45"
                    Text {
                        id: statusText
                        anchors.centerIn: parent
                        text: controller.statusText
                        font.pixelSize: 14
                        color: "#a6e3a1"
                    }
                }
            }

            // ── Snapshot + transaction, side by side ──────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                // ── Monthly snapshot / accounts ───────────────────────────────
                ChartCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 3
                    implicitHeight: snapshotCol.implicitHeight + 50
                    title: "Monthly snapshot — " + Qt.formatDate(new Date(), "MMMM yyyy")

                    ColumnLayout {
                        id: snapshotCol
                        anchors.fill: parent
                        spacing: 0

                        // Table header
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            color: "#181825"
                            radius: 4
                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                spacing: 10
                                Text { Layout.preferredWidth: 110; text: "CATEGORY"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.fillWidth: true;     text: "ACCOUNT";  font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.preferredWidth: 150; text: "BALANCE";  font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            }
                        }

                        Repeater {
                            model: AccountController.accounts
                            delegate: Rectangle {
                                id: accountRow
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                color: index % 2 === 0 ? "transparent" : "#181825"

                                RowLayout {
                                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                    spacing: 10

                                    Rectangle {
                                        Layout.preferredWidth: 110
                                        Layout.preferredHeight: 20
                                        radius: 10
                                        color: Qt.rgba(1, 1, 1, 0.08)
                                        border.color: page.categoryBadgeColor(accountRow.modelData.category)
                                        Text {
                                            anchors.centerIn: parent
                                            text: accountRow.modelData.category.length > 0 ? accountRow.modelData.category : "—"
                                            font.pixelSize: 12
                                            color: page.categoryBadgeColor(accountRow.modelData.category)
                                            elide: Text.ElideRight
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Text {
                                            text: accountRow.modelData.name
                                            font.pixelSize: 15
                                            color: "#cdd6f4"
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: accountRow.modelData.type
                                            font.pixelSize: 12
                                            color: "#6c7086"
                                        }
                                    }
                                    TextField {
                                        id: balanceField
                                        Layout.preferredWidth: 150
                                        placeholderText: "0,00 €"
                                        placeholderTextColor: "#45475a"
                                        horizontalAlignment: TextInput.AlignRight
                                        color: "#cdd6f4"
                                        font.pixelSize: 15
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        background: Rectangle {
                                            color: "#11111b"
                                            radius: 4
                                            border.color: balanceField.activeFocus ? "#a6e3a1" : "#313244"
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            wrapMode: Text.WordWrap
                            text: "Add, edit, or remove accounts from Data Entry → \"Manage Accounts\"."
                            font.pixelSize: 13
                            color: "#6c7086"
                        }

                        // Footer: save snapshot
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 10
                            spacing: 10

                            Item { Layout.fillWidth: true }
                            Rectangle {
                                width: 130; height: 30; radius: 4
                                color: snapSaveHover.hovered ? "#264d80" : "#1e3a5f"
                                border.color: "#89b4fa"
                                Text {
                                    anchors.centerIn: parent
                                    text: "Save snapshot"
                                    font.pixelSize: 15
                                    font.bold: true
                                    color: "#cdd6f4"
                                }
                                HoverHandler { id: snapSaveHover }
                                TapHandler { onTapped: controller.saveSnapshot() }
                            }
                        }
                    }
                }

                // ── Transaction entry ─────────────────────────────────────────
                ChartCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 2
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: txCol.implicitHeight + 50
                    title: "Add transaction"

                    ColumnLayout {
                        id: txCol
                        anchors.fill: parent
                        spacing: 10

                        TextField {
                            id: txLabel
                            Layout.fillWidth: true
                            placeholderText: "Label  (e.g. Carrefour, EDF, SNCF…)"
                            placeholderTextColor: "#45475a"
                            color: "#cdd6f4"
                            font.pixelSize: 15
                            background: Rectangle {
                                color: "#11111b"
                                radius: 4
                                border.color: txLabel.activeFocus ? "#89b4fa" : "#313244"
                            }
                        }

                        RowLayout {
                            spacing: 10
                            TextField {
                                id: txAmount
                                Layout.preferredWidth: 120
                                placeholderText: "-0,00 €"
                                placeholderTextColor: "#45475a"
                                horizontalAlignment: TextInput.AlignRight
                                color: "#cdd6f4"
                                font.pixelSize: 15
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                background: Rectangle {
                                    color: "#11111b"
                                    radius: 4
                                    border.color: txAmount.activeFocus ? "#a6e3a1" : "#313244"
                                }
                            }
                            ComboBox {
                                id: txCategory
                                Layout.preferredWidth: 140
                                model: CategoryController.categories.map(function(c) { return c.name })
                            }
                            ComboBox {
                                id: txSubcategory
                                Layout.fillWidth: true
                                readonly property var subNames: {
                                    var cat = CategoryController.categories.find(function(c) { return c.name === txCategory.currentText })
                                    return cat ? cat.subcategories.map(function(s) { return s.name }) : []
                                }
                                model: subNames
                                enabled: subNames.length > 0
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Pick a category and subcategory — manage the list from Data Entry → \"Manage Categories\"."
                            wrapMode: Text.WordWrap
                            font.pixelSize: 13
                            color: "#6c7086"
                        }

                        Rectangle {
                            Layout.alignment: Qt.AlignRight
                            width: 80; height: 30; radius: 4
                            color: txAddHover.hovered ? "#264d80" : "#1e3a5f"
                            border.color: "#89b4fa"
                            Text {
                                anchors.centerIn: parent
                                text: "Add"
                                font.pixelSize: 15
                                font.bold: true
                                color: "#cdd6f4"
                            }
                            HoverHandler { id: txAddHover }
                            TapHandler { onTapped: controller.addTransaction() }
                        }
                    }
                }
            }

            // ── History ───────────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: historyCol.implicitHeight + 50
                title: "History — past entries (editing / re-categorizing comes later)"

                ColumnLayout {
                    id: historyCol
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        color: "#181825"
                        radius: 4
                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            Text { Layout.preferredWidth: 90;  text: "DATE";     font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            Text { Layout.fillWidth: true;     text: "LABEL";    font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            Text { Layout.preferredWidth: 110; text: "AMOUNT";   font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 120; text: "CATEGORY"; font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                        }
                    }

                    Repeater {
                        model: controller.historyRows
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            color: index % 2 === 0 ? "transparent" : "#181825"

                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                Text { Layout.preferredWidth: 90;  text: modelData.date;  font.pixelSize: 15; color: "#6c7086" }
                                Text { Layout.fillWidth: true;     text: modelData.label; font.pixelSize: 15; color: "#cdd6f4"; elide: Text.ElideRight }
                                Text {
                                    Layout.preferredWidth: 110
                                    text: modelData.amount
                                    font.pixelSize: 15
                                    color: modelData.amount.startsWith("+") ? "#a6e3a1" : "#cdd6f4"
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text { Layout.preferredWidth: 120; text: modelData.category; font.pixelSize: 14; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            }
                        }
                    }
                }
            }

            // ── Annual summary ────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: annualCol.implicitHeight + 50
                title: "Annual summary — year vs year"

                ColumnLayout {
                    id: annualCol
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        color: "#181825"
                        radius: 4
                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            Text { Layout.preferredWidth: 70;  text: "YEAR";         font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            Text { Layout.fillWidth: true;     text: "INCOME";       font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 110; text: "EXPENSES";     font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 110; text: "SAVED";        font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 110; text: "SAVINGS RATE"; font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                        }
                    }

                    Repeater {
                        model: controller.annualRows
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            color: index % 2 === 0 ? "transparent" : "#181825"

                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                Text { Layout.preferredWidth: 70;  text: modelData.year;     font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                                Text { Layout.fillWidth: true;     text: modelData.income;   font.pixelSize: 15; color: "#a6e3a1"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 110; text: modelData.expenses; font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 110; text: modelData.saved;    font.pixelSize: 15; color: "#cdd6f4"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 110; text: modelData.rate;     font.pixelSize: 15; color: "#89b4fa"; horizontalAlignment: Text.AlignRight }
                            }
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }

}