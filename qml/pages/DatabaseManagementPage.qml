import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Database Management — own sidebar category, no sub-actions ───────────────
// Lets the developer/user wipe the SQLite file back to a blank slate
// ("Reset Database") or wipe-then-reseed a small demo dataset ("Reset to
// Mock Data") — useful whenever the schema changes during development (see
// ARCHITECTURE.MD § Database Management). Backed by
// DatabaseManagementController (one per page, not a QML_SINGLETON — no
// state to keep alive). Both actions are destructive and irreversible, so
// both are gated behind a confirmation dialog first.
//
// After either action, CategoryController.reload() and
// AccountController.reload() are called here (not by the controller itself
// — see DatabaseManagementController's class comment) so those singletons'
// cached lists pick up the wipe/reseed immediately instead of only on next
// app launch.

Item {
    id: page

    DatabaseManagementController {
        id: controller
        onDatabaseReset: {
            CategoryController.reload()
            AccountController.reload()
        }
    }

    ColumnLayout {
        x: 20; y: 20
        width: parent.width - 40
        spacing: 16

        RowLayout {
            spacing: 12
            Text {
                text: "Database Management"
                font.pixelSize: 23
                font.bold: true
                color: "#cdd6f4"
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                visible: controller.statusText.length > 0
                width: statusLabel.implicitWidth + 20
                height: 26
                radius: 13
                color: "#1a3328"
                border.color: "#2d5a45"
                Text {
                    id: statusLabel
                    anchors.centerIn: parent
                    text: controller.statusText
                    font.pixelSize: 14
                    color: "#a6e3a1"
                }
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Both actions wipe every table in the local SQLite database. Useful when the schema "
                + "changes during development, or to start over. Neither action can be undone."
            font.pixelSize: 15
            color: "#6c7086"
        }

        ChartCard {
            Layout.fillWidth: true
            implicitHeight: actionsCol.implicitHeight + 50
            title: "Reset actions"

            ColumnLayout {
                id: actionsCol
                anchors.fill: parent
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "Reset Database"; font.pixelSize: 16; font.bold: true; color: "#cdd6f4" }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Clears everything — accounts, categories, hashes, transactions. Starts from a "
                                + "completely blank database."
                            font.pixelSize: 13
                            color: "#6c7086"
                        }
                    }
                    Rectangle {
                        width: 150; height: 32; radius: 4
                        color: resetHover.hovered ? "#5a2d36" : "#3d2028"
                        border.color: "#f38ba8"
                        Text { anchors.centerIn: parent; text: "Reset Database"; font.pixelSize: 14; font.bold: true; color: "#f38ba8" }
                        HoverHandler { id: resetHover }
                        TapHandler { onTapped: confirmDialog.openFor("reset") }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "Reset to Mock Data"; font.pixelSize: 16; font.bold: true; color: "#cdd6f4" }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Clears everything, then reseeds the default categories, account categories, "
                                + "account types, demo accounts, and a few sample transactions."
                            font.pixelSize: 13
                            color: "#6c7086"
                        }
                    }
                    Rectangle {
                        width: 170; height: 32; radius: 4
                        color: mockHover.hovered ? "#264d80" : "#1e3a5f"
                        border.color: "#89b4fa"
                        Text { anchors.centerIn: parent; text: "Reset to Mock Data"; font.pixelSize: 14; font.bold: true; color: "#cdd6f4" }
                        HoverHandler { id: mockHover }
                        TapHandler { onTapped: confirmDialog.openFor("mock") }
                    }
                }
            }
        }

        Item { height: 8 }
    }

    // ── Confirmation dialog (shared by both actions) ──────────────────────────
    Popup {
        id: confirmDialog
        anchors.centerIn: parent
        width: 380
        padding: 20
        modal: true

        property string mode: "reset" // "reset" | "mock"

        function openFor(m) { mode = m; open() }

        background: Rectangle { color: "#181825"; border.color: "#45475a"; radius: 8 }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: confirmDialog.mode === "reset" ? "Reset the database?" : "Reset to mock data?"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: confirmDialog.mode === "reset"
                    ? "This permanently deletes every account, category, hash, and transaction. This cannot be undone."
                    : "This permanently deletes everything currently stored, then reloads the default demo data. This cannot be undone."
                font.pixelSize: 14
                color: "#a6adc8"
            }
            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: cancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: cancelHover }
                    TapHandler { onTapped: confirmDialog.close() }
                }
                Rectangle {
                    width: 140; height: 30; radius: 4
                    color: confirmHover.hovered ? "#5a2d36" : "#3d2028"
                    border.color: "#f38ba8"
                    Text {
                        anchors.centerIn: parent
                        text: confirmDialog.mode === "reset" ? "Reset" : "Reset to mock"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#f38ba8"
                    }
                    HoverHandler { id: confirmHover }
                    TapHandler {
                        onTapped: {
                            if (confirmDialog.mode === "reset")
                                controller.resetDatabase()
                            else
                                controller.resetToMock()
                            confirmDialog.close()
                        }
                    }
                }
            }
        }
    }
}
