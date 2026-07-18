pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Account Details — Overview → "Account Details" ────────────────────────────
// Read-only per-account view: current balance, a day-by-day balance-over-time
// line chart, and the account's transaction list — the "not to manage, but to
// see history" page from the product request. Deliberately has no edit/
// delete/add actions anywhere on it; that's AccountsManagePage.qml /
// AccountTransactionsPage.qml's job (reached from Manage Accounts). Lives
// under the **Overview** sidebar category (moved here from Account
// Management on product feedback: this is a day-to-day analysis view, not
// an admin/CRUD operation, so it belongs with the other "look at my data"
// pages rather than the account-admin category) — reached directly from the
// sidebar via its own "Account Details" action, not drilled into from
// Manage Accounts, since it's a standalone view over any account (has its
// own account picker), not tied to a specific row's button. File/QML type
// name is still `AccountHistoryPage` — only the sidebar label/id changed,
// same "renamed label, unchanged class name" precedent as
// `SnapshotImportController` backing "Import Transactions" — see
// ARCHITECTURE.MD § Account History.
//
// Backed by AccountController (QML_SINGLETON) directly, same as
// AccountTransactionsPage.qml — no dedicated controller for this page, since
// it only reads account-domain data that already lives on AccountController
// (accountBalanceHistory()/accountBalanceSummary(), see that class).
//
// Chart is the same GraphsView/DateTimeAxis/ValueAxis/LineSeries recipe as
// SavingsPage.qml's "Balance over time" chart (verified against that page,
// not invented — see ARCHITECTURE.MD header note on checking
// QtGraphsExamples/doc.qt.io first), with one difference: the LineSeries here
// is populated imperatively (append()/clear() in rebuildChart()) rather than
// once in Component.onCompleted, because the account picker can change which
// account's history is plotted without the page itself being recreated.

Item {
    id: page

    signal closeRequested()

    property int selectedAccountIndex: AccountController.accounts.length > 0 ? 0 : -1

    readonly property var account:
        page.selectedAccountIndex >= 0 && page.selectedAccountIndex < AccountController.accounts.length
            ? AccountController.accounts[page.selectedAccountIndex] : null

    property var summary: ({})
    property var history: []
    property var rows: []

    function reload() {
        if (page.selectedAccountIndex >= 0) {
            page.summary = AccountController.accountBalanceSummary(page.selectedAccountIndex)
            page.history = AccountController.accountBalanceHistory(page.selectedAccountIndex)
            page.rows = AccountController.transactionsForAccount(page.selectedAccountIndex)
        } else {
            page.summary = ({})
            page.history = []
            page.rows = []
        }
        rebuildChart()
    }

    function rebuildChart() {
        balanceSeries.clear()
        for (var i = 0; i < page.history.length; i++) {
            balanceSeries.append(page.history[i].timeMs, page.history[i].balance)
        }
    }

    Component.onCompleted: page.reload()
    onSelectedAccountIndexChanged: page.reload()

    // Crash fix: an account with only one transaction *date* (very common —
    // e.g. a single imported/edited row) makes minTimeMs === maxTimeMs below
    // if left unpadded. DateTimeAxis's docs don't forbid an equal min/max,
    // but Qt Graphs' automatic tick-layout ("If [tickInterval] is 0 or lower,
    // axis will automatically calculate appropriate amount of ticks") almost
    // certainly divides by the axis range internally — a zero-width range
    // reproducibly crashed the app with no QML-visible error (a native
    // crash bypasses QML's own warning/error console entirely, unlike a JS
    // exception). Padding by one day whenever the raw range collapses to
    // zero keeps the axis strictly positive-width in every case.
    readonly property double minTimeMs: page.history.length > 0 ? page.history[0].timeMs : 0
    readonly property double maxTimeMsRaw: page.history.length > 0 ? page.history[page.history.length - 1].timeMs : 86400000
    readonly property double maxTimeMs: page.maxTimeMsRaw > page.minTimeMs ? page.maxTimeMsRaw : page.minTimeMs + 86400000
    readonly property double balanceAxisMin: {
        if (page.history.length === 0) return 0
        var m = page.history[0].balance
        for (var i = 1; i < page.history.length; i++) m = Math.min(m, page.history[i].balance)
        return Math.min(m * 1.1, 0)
    }
    readonly property double balanceAxisMax: {
        if (page.history.length === 0) return 1
        var m = page.history[0].balance
        for (var i = 1; i < page.history.length; i++) m = Math.max(m, page.history[i].balance)
        return m > 0 ? m * 1.1 : 1
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            PageHeader {
                showBack: true
                title: "Account Details" + (page.account !== null ? " — " + page.account.name : "")
                onBackClicked: page.closeRequested()

                ComboBox {
                    Layout.preferredWidth: 240
                    model: AccountController.accounts
                    textRole: "name"
                    currentIndex: page.selectedAccountIndex
                    onActivated: page.selectedAccountIndex = currentIndex
                }
            }

            Text {
                visible: AccountController.accounts.length === 0
                text: "No accounts yet — add one from Account Management → Manage Accounts."
                font.pixelSize: 15
                color: "#f38ba8"
            }

            ColumnLayout {
                visible: page.account !== null
                Layout.fillWidth: true
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    KpiCard {
                        Layout.fillWidth: true
                        label: "Current balance"
                        value: page.summary.currentBalanceText ? page.summary.currentBalanceText : "—"
                        sub: page.summary.asOfDateText ? ("as of " + page.summary.asOfDateText) : ""
                    }
                    KpiCard {
                        Layout.fillWidth: true
                        label: "Transactions"
                        value: page.summary.transactionCount !== undefined ? String(page.summary.transactionCount) : "0"
                        sub: ""
                    }
                    KpiCard {
                        Layout.fillWidth: true
                        label: "Type / Category"
                        value: page.account !== null ? page.account.type : "—"
                        sub: page.account !== null ? page.account.category : ""
                    }
                }

                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: 340
                    title: "Balance over time (€) — computed from every transaction"

                    GraphsView {
                        anchors.fill: parent
                        visible: page.history.length > 0
                        theme: GraphsTheme {
                            colorScheme: GraphsTheme.ColorScheme.Dark
                            plotAreaBackgroundVisible: true
                        }

                        axisX: DateTimeAxis {
                            min: new Date(page.minTimeMs)
                            max: new Date(page.maxTimeMs)
                            labelFormat: "MMM yy"
                        }
                        axisY: ValueAxis {
                            min: page.balanceAxisMin
                            max: page.balanceAxisMax
                            labelFormat: "%.0f"
                        }

                        LineSeries {
                            id: balanceSeries
                            name: page.account !== null ? page.account.name : ""
                            color: "#89b4fa"
                            width: 2
                            pointDelegate: Component {
                                HoverPoint {
                                    seriesName: page.account !== null ? page.account.name : ""
                                    pointColor: "#89b4fa"
                                }
                            }
                        }
                    }

                    EmptyState {
                        visible: page.history.length === 0
                        anchors.centerIn: parent
                        message: "No dated transactions to chart yet for this account."
                    }
                }

                // ── Transaction history table (read-only, no edit/delete) ───────
                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: txCol.implicitHeight + 50
                    title: "Transaction history"

                    ColumnLayout {
                        id: txCol
                        anchors.fill: parent
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            color: "#181825"
                            radius: 4
                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                spacing: 8
                                Text { Layout.preferredWidth: 90;  text: "DATE";        font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.preferredWidth: 300; text: "NAME";        font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Item { Layout.fillWidth: true }
                                Text { Layout.preferredWidth: 90;  text: "DEBIT";       font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 90;  text: "CREDIT";      font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 100; text: "BALANCE";     font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 130; text: "CATEGORY";    font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.preferredWidth: 130; text: "SUBCATEGORY"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                            }
                        }

                        EmptyState {
                            visible: page.rows.length === 0
                            Layout.topMargin: 10
                            message: "No transactions saved for this account yet."
                        }

                        Repeater {
                            model: page.rows
                            delegate: Rectangle {
                                id: txRow
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                color: index % 2 === 0 ? "transparent" : "#181825"

                                RowLayout {
                                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                    spacing: 8
                                    Text { Layout.preferredWidth: 90; text: txRow.modelData.operationDate; font.pixelSize: 14; color: "#6c7086" }
                                    Text {
                                        Layout.preferredWidth: 300
                                        text: txRow.modelData.name.length > 0 ? txRow.modelData.name : "—"
                                        font.pixelSize: 15
                                        color: "#cdd6f4"
                                        elide: Text.ElideRight
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text { Layout.preferredWidth: 90;  text: txRow.modelData.debitText;  font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 90;  text: txRow.modelData.creditText; font.pixelSize: 15; color: "#a6e3a1"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 100; text: txRow.modelData.balanceText; font.pixelSize: 14; color: "#6c7086"; horizontalAlignment: Text.AlignRight }

                                    Rectangle {
                                        Layout.preferredWidth: 130
                                        visible: txRow.modelData.category.length > 0
                                        Layout.preferredHeight: 20
                                        radius: 10
                                        color: Qt.rgba(1, 1, 1, 0.08)
                                        border.color: CategoryController.categoryColor(txRow.modelData.category) || "#6c7086"
                                        Text {
                                            anchors.centerIn: parent
                                            text: txRow.modelData.category
                                            font.pixelSize: 12
                                            color: CategoryController.categoryColor(txRow.modelData.category) || "#6c7086"
                                            elide: Text.ElideRight
                                            width: Math.min(implicitWidth, 118)
                                        }
                                    }
                                    Text { Layout.preferredWidth: 130; visible: txRow.modelData.category.length === 0; text: "—"; font.pixelSize: 14; color: "#6c7086" }

                                    Rectangle {
                                        Layout.preferredWidth: 130
                                        visible: txRow.modelData.subcategory.length > 0
                                        Layout.preferredHeight: 20
                                        radius: 10
                                        color: Qt.rgba(1, 1, 1, 0.08)
                                        border.color: CategoryController.subcategoryColor(txRow.modelData.category, txRow.modelData.subcategory) || "#6c7086"
                                        Text {
                                            anchors.centerIn: parent
                                            text: txRow.modelData.subcategory
                                            font.pixelSize: 12
                                            color: CategoryController.subcategoryColor(txRow.modelData.category, txRow.modelData.subcategory) || "#6c7086"
                                            elide: Text.ElideRight
                                            width: Math.min(implicitWidth, 118)
                                        }
                                    }
                                    Text { Layout.preferredWidth: 130; visible: txRow.modelData.subcategory.length === 0; text: "—"; font.pixelSize: 14; color: "#6c7086" }
                                }
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
