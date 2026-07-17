import QtQuick
import QtQuick.Layouts
import FinanceDashboard

// ── Database Management — own sidebar category, no sub-actions ───────────────
// Lets the developer/user wipe the SQLite file back to a blank slate
// ("Reset Database") or wipe-then-reseed a small demo dataset ("Reset to
// Mock Data") — useful whenever the schema changes during development (see
// ARCHITECTURE.MD § Database Management). Backed by
// DatabaseManagementController (one per page, not a QML_SINGLETON — no
// state to keep alive). Both actions are destructive and irreversible, so
// both are gated behind a confirmation dialog first — both dialogs use the
// red/danger confirm styling regardless of the trigger button's own color
// (Reset to Mock Data's trigger is blue/primary, its confirm step is still
// red, deliberately, since it's just as irreversible as a full reset).
//
// After either action, CategoryController.reload() and
// AccountController.reload() are called here (not by the controller itself
// — see DatabaseManagementController's class comment) so those singletons'
// cached lists pick up the wipe/reseed immediately instead of only on next
// app launch.
//
// Built from the shared qml/components/ (PageHeader, StatusBadge,
// PillButton, ConfirmDialog) — see ARCHITECTURE.MD § Shared Components.

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

        PageHeader {
            title: "Database Management"

            StatusBadge {
                visible: controller.statusText.length > 0
                text: controller.statusText
                variant: "success"
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
                    PillButton {
                        text: "Reset Database"
                        variant: "danger"
                        Layout.preferredHeight: 32
                        onClicked: confirmDialog.openFor("reset")
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#313244" }

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
                    PillButton {
                        text: "Reset to Mock Data"
                        variant: "primary"
                        Layout.preferredHeight: 32
                        onClicked: confirmDialog.openFor("mock")
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }

    // ── Confirmation dialog (shared by both actions) ──────────────────────────
    ConfirmDialog {
        id: confirmDialog
        width: 380

        property string mode: "reset" // "reset" | "mock"

        function openFor(m) {
            mode = m
            dialogTitle = m === "reset" ? "Reset the database?" : "Reset to mock data?"
            message = m === "reset"
                ? "This permanently deletes every account, category, hash, and transaction. This cannot be undone."
                : "This permanently deletes everything currently stored, then reloads the default demo data. This cannot be undone."
            confirmText = m === "reset" ? "Reset" : "Reset to mock"
            open()
        }

        onConfirmed: {
            if (mode === "reset")
                controller.resetDatabase()
            else
                controller.resetToMock()
        }
    }
}
