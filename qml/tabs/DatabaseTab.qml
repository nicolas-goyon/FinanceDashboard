import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import FinanceDashboard

// ── Database tab ──────────────────────────────────────────────────────────────
// Raw table editor for `snapshots` and `asset_entries`.
// Edits are buffered (OnManualSubmit) until Save. Double-click a cell to edit.
// The id column (0) is read-only — enforced in C++ (DbTableModel::flags).

Item {
    id: root

    signal saved()

    DatabaseController {
        id: controller
        onChangesSaved: root.saved()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        // ── Header ────────────────────────────────────────────────────────────
        RowLayout {
            Text {
                text: "Database"
                font { pixelSize: 23; bold: true }
                color: "#cdd6f4"
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 100; height: 30; radius: 4
                color: refreshHover.hovered ? "#313244" : "#1e1e2e"
                border.color: "#313244"
                Text { anchors.centerIn: parent; text: "↻  Refresh"; font.pixelSize: 15; color: "#cdd6f4" }
                HoverHandler { id: refreshHover }
                TapHandler { onTapped: controller.refresh() }
            }
        }

        // ── Table selector ────────────────────────────────────────────────────
        RowLayout {
            spacing: 10
            Text { text: "Table:"; font.pixelSize: 15; color: "#6c7086" }
            ComboBox {
                id: tableSelector
                model: ["snapshots", "asset_entries"]
                implicitWidth: 180
                onActivated: (index) => controller.currentTable = index
            }
            Text {
                text: controller.rowCount + (controller.rowCount === 1 ? " row" : " rows")
                font.pixelSize: 14
                color: "#6c7086"
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── Table ─────────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e2e"
            border.color: "#313244"
            radius: 6
            clip: true

            HorizontalHeaderView {
                id: header
                anchors { top: parent.top; left: parent.left; right: parent.right; margins: 1 }
                syncView: table
                clip: true
            }

            TableView {
                id: table
                anchors {
                    top: header.bottom; bottom: parent.bottom
                    left: parent.left; right: parent.right
                    margins: 1
                }
                clip: true
                model: controller.model
                selectionBehavior: TableView.SelectRows
                selectionModel: ItemSelectionModel { model: table.model }
                columnWidthProvider: function (column) {
                    return controller.columnWidth(column)
                }

                delegate: Rectangle {
                    // NOTE: once a delegate declares required properties, model
                    // roles are ONLY delivered through required properties —
                    // so "display" must be declared too.
                    required property var display
                    required property int row
                    required property int column
                    required property bool selected
                    required property bool editing

                    implicitWidth: 120
                    implicitHeight: 30
                    color: selected ? "#2a2a40" : "#1e1e2e"

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        verticalAlignment: Text.AlignVCenter
                        visible: !editing
                        text: display ?? ""
                        elide: Text.ElideRight
                        font.pixelSize: 15
                        color: column === 0 ? "#6c7086" : "#cdd6f4"
                    }

                    Rectangle {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                        height: 1
                        color: "#313244"
                    }

                    TapHandler {
                        onTapped: {
                            const mi = table.model.index(row, column)
                            table.selectionModel.setCurrentIndex(mi, ItemSelectionModel.NoUpdate)
                            table.selectionModel.select(mi,
                                ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
                        }
                    }

                    TableView.editDelegate: TextField {
                        anchors.fill: parent
                        text: display ?? ""
                        color: "#cdd6f4"
                        font.pixelSize: 15
                        background: Rectangle { color: "#11111b"; border.color: "#89b4fa" }
                        Component.onCompleted: selectAll()
                        TableView.onCommit: display = text
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

        // ── Bottom action bar ─────────────────────────────────────────────────
        RowLayout {
            spacing: 8

            Rectangle {
                width: 110; height: 30; radius: 4
                color: addHover.hovered ? "#313244" : "#1e1e2e"
                border.color: "#313244"
                Text { anchors.centerIn: parent; text: "＋  Add Row"; font.pixelSize: 15; color: "#cdd6f4" }
                HoverHandler { id: addHover }
                TapHandler { onTapped: controller.addRow() }
            }
            Rectangle {
                width: 140; height: 30; radius: 4
                color: delHover.hovered ? "#3d2028" : "transparent"
                border.color: delHover.hovered ? "#f38ba8" : "#313244"
                Text {
                    anchors.centerIn: parent
                    text: "✕  Delete Selected"
                    font.pixelSize: 15
                    color: "#f38ba8"
                }
                HoverHandler { id: delHover }
                TapHandler {
                    onTapped: {
                        const indexes = table.selectionModel.selectedIndexes
                        let rows = []
                        for (let i = 0; i < indexes.length; i++) {
                            if (rows.indexOf(indexes[i].row) === -1)
                                rows.push(indexes[i].row)
                        }
                        controller.deleteRows(rows)
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: controller.statusText
                font.pixelSize: 14
                color: controller.statusIsError ? "#f38ba8" : "#a6e3a1"
            }

            Rectangle {
                width: 80; height: 30; radius: 4
                color: revertHover.hovered ? "#313244" : "#1e1e2e"
                border.color: "#313244"
                Text { anchors.centerIn: parent; text: "Revert"; font.pixelSize: 15; color: "#cdd6f4" }
                HoverHandler { id: revertHover }
                TapHandler { onTapped: controller.revert() }
            }
            Rectangle {
                width: 120; height: 30; radius: 4
                color: saveHover.hovered ? "#264d80" : "#1e3a5f"
                border.color: "#89b4fa"
                Text { anchors.centerIn: parent; text: "Save Changes"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                HoverHandler { id: saveHover }
                TapHandler { onTapped: controller.save() }
            }
        }
    }
}
