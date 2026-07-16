import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Data Entry / Reports — PROJECT.md page 6 ──────────────────────────────────
// Monthly snapshot per account (with account management) · transaction entry
// with manual category/subcategory picker (CategoryController — see Data
// Entry → "Manage Categories" / CategoriesManagePage.qml) · history · annual
// summary
// [MOCK — DataEntryController; nothing is persisted in the UI pass]

Item {
    id: page

    DataEntryController { id: controller }

    readonly property var typeColors: ({
        "Checking": { bg: "#1e3a5f", fg: "#89b4fa" },
        "Savings":  { bg: "#1a3328", fg: "#a6e3a1" },
        "Trading":  { bg: "#2d2440", fg: "#cba6f7" }
    })

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
                                Text { Layout.preferredWidth: 80;  text: "TYPE";    font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.fillWidth: true;     text: "ACCOUNT"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.preferredWidth: 150; text: "BALANCE"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Item { Layout.preferredWidth: 26 }
                            }
                        }

                        Repeater {
                            model: controller.accounts
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
                                        Layout.preferredWidth: 80
                                        Layout.preferredHeight: 20
                                        radius: 10
                                        color: page.typeColors[accountRow.modelData.type].bg
                                        Text {
                                            anchors.centerIn: parent
                                            text: accountRow.modelData.type
                                            font.pixelSize: 13
                                            color: page.typeColors[accountRow.modelData.type].fg
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: accountRow.modelData.name
                                        font.pixelSize: 15
                                        color: "#cdd6f4"
                                        elide: Text.ElideRight
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
                                    Rectangle {
                                        Layout.preferredWidth: 26
                                        Layout.preferredHeight: 26
                                        radius: 4
                                        color: removeHover.hovered ? "#3d2028" : "transparent"
                                        border.color: removeHover.hovered ? "#f38ba8" : "#313244"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            font.pixelSize: 17
                                            font.bold: true
                                            color: removeHover.hovered ? "#f38ba8" : "#6c7086"
                                        }
                                        HoverHandler { id: removeHover }
                                        TapHandler {
                                            onTapped: controller.removeAccount(accountRow.index)
                                        }
                                    }
                                }
                            }
                        }

                        // Footer: add account + save snapshot
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 10
                            spacing: 10

                            Rectangle {
                                width: 130; height: 30; radius: 4
                                color: addAccountHover.hovered ? "#181825" : "transparent"
                                border.color: "#313244"
                                Text {
                                    anchors.centerIn: parent
                                    text: "＋  Add account"
                                    font.pixelSize: 15
                                    color: "#89b4fa"
                                }
                                HoverHandler { id: addAccountHover }
                                TapHandler { onTapped: addAccountDialog.open() }
                            }
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

    // ── Add-account dialog ────────────────────────────────────────────────────
    Popup {
        id: addAccountDialog
        anchors.centerIn: parent
        width: 380
        padding: 20
        modal: true

        background: Rectangle {
            color: "#181825"
            border.color: "#45475a"
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "New account"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }

            RowLayout {
                spacing: 10
                Text { Layout.preferredWidth: 60; text: "Name"; font.pixelSize: 15; color: "#6c7086" }
                TextField {
                    id: accountNameField
                    Layout.fillWidth: true
                    placeholderText: "e.g. Livret Jeune"
                    placeholderTextColor: "#45475a"
                    color: "#cdd6f4"
                    font.pixelSize: 15
                    background: Rectangle {
                        color: "#11111b"
                        radius: 4
                        border.color: accountNameField.activeFocus ? "#89b4fa" : "#313244"
                    }
                }
            }
            RowLayout {
                spacing: 10
                Text { Layout.preferredWidth: 60; text: "Type"; font.pixelSize: 15; color: "#6c7086" }
                ComboBox {
                    id: accountTypeField
                    Layout.fillWidth: true
                    model: controller.accountTypes
                }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: accCancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: accCancelHover }
                    TapHandler { onTapped: addAccountDialog.close() }
                }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: accAddHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text { anchors.centerIn: parent; text: "Add"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                    HoverHandler { id: accAddHover }
                    TapHandler {
                        onTapped: {
                            var trimmed = accountNameField.text.trim()
                            if (trimmed.length === 0)
                                return
                            controller.addAccount(trimmed, accountTypeField.currentText)
                            addAccountDialog.close()
                        }
                    }
                }
            }
        }
    }
}