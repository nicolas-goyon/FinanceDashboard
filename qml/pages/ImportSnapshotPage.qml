pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import QtGraphs
import FinanceDashboard

// ── Import Transactions — statement import ────────────────────────────────────
// Reached from the sidebar: Transactions → "Import Transactions" action
// (renamed from "New Monthly Snapshot" / "Data Entry"; category renamed from
// "Transactions Management" 2026-07-18 — see ARCHITECTURE.MD § Transactions).
// Step 1: pick a template + file → real parsing (SnapshotImportController,
//         backed by src/import/ — see ARCHITECTURE.MD § Statement Import).
// Step 2: review header info, match/pick the target account, edit rows and
//         categories, inspect the parse log, save for real (see
//         SnapshotImportController class comment for scope — persists to
//         the `transactions` table and auto-links the account hash). Save
//         is gated on a reconciliation check first — see requestSave()/
//         reconciliationDialog below and ARCHITECTURE.MD § Statement
//         Import — Import Report.
// Step 3: import report — income/expense/net + category breakdown of what
//         was just saved, plus the reconciliation results. Still the same
//         Item/controller (not a separate overlay page) so
//         SnapshotImportController's state survives the transition.

Item {
    id: page

    signal closeRequested()

    property int step: 0   // 0 = pick template/file · 1 = review/edit · 2 = import report

    SnapshotImportController { id: controller }
    // The worker thread of the prediction (#52) rebuilds the rows when its
    // suggestions come. A NAME field with the focus would lose the text that
    // the user typed, so it gives the text to setRowName() first: it loses the
    // focus (focus = false, also inside a FocusScope), and its
    // editingFinished handler runs. The new field does not get the focus.
    Connections {
        target: controller
        function onPredictionsAboutToBeApplied() {
            const focused = page.Window.activeFocusItem
            if (focused !== null && focused.objectName === "rowNameField")
                focused.focus = false
        }
    }
    // Accounts now come from AccountController (QML_SINGLETON,
    // src/controllers/dataentry/AccountController.h) — the real, DB-backed
    // accounts list (see ARCHITECTURE.MD § Account Management), same
    // singleton read by AccountsManagePage.qml/AccountTypesCategoriesPage.qml/
    // AccountTransactionsPage.qml.
    // Category/subcategory come from the CategoryController singleton
    // instead (see qml/pages/CategoriesManagePage.qml).

    property int currentTemplateIndex: 0
    readonly property var currentTemplate:
        controller.templates.length > 0 ? controller.templates[currentTemplateIndex] : null

    property int matchedAccountIndex: -1
    property int selectedAccountIndex: -1
    // The id of the picked account. The indexes above are positions in
    // AccountController.accounts; this page stays loaded while another page
    // adds or removes accounts (#64), so the indexes are read again from the
    // ids when the list changes (see the Connections below).
    property int selectedAccountId: -1

    readonly property var selectedAccount:
        page.selectedAccountIndex >= 0 && page.selectedAccountIndex < AccountController.accounts.length
            ? AccountController.accounts[page.selectedAccountIndex] : null
    // The account that the tag of the statement suggests (-1 when no account
    // or two accounts have it, #203).
    readonly property string matchedAccountName:
        page.matchedAccountIndex >= 0 && page.matchedAccountIndex < AccountController.accounts.length
            ? AccountController.accounts[page.matchedAccountIndex].name : ""

    function readMatches() {
        matchedAccountIndex = controller.matchAccountIndex(AccountController.accounts)
    }

    function goToReview() {
        readMatches()
        selectAccountIndex(matchedAccountIndex)
        page.step = 1
    }

    function selectAccountIndex(index) {
        selectedAccountIndex = index
        selectedAccountId = index >= 0 && index < AccountController.accounts.length
            ? AccountController.accounts[index].id : -1
    }

    Connections {
        target: AccountController
        function onAccountsChanged() {
            if (page.step !== 1)
                return
            page.readMatches()
            var accounts = AccountController.accounts
            var index = -1
            for (var i = 0; i < accounts.length; i++) {
                if (accounts[i].id === page.selectedAccountId) {
                    index = i
                    break
                }
            }
            // A removed account is not picked again: Save stays disabled
            // until the user picks one.
            page.selectAccountIndex(index)
        }
    }

    // ── Save flow — reconciliation gate + brief disable + report step ────────
    // Disabling the Save button for a couple of seconds after it's clicked
    // is a double-click guard (see saveButtonResetTimer below); the
    // reconciliation check runs first and can interrupt the flow entirely
    // (see reconciliationDialog) — see ARCHITECTURE.MD § Statement Import —
    // Import Report.
    property bool saveBusy: false

    Timer {
        id: saveButtonResetTimer
        interval: 2000
        onTriggered: page.saveBusy = false
    }

    function requestSave() {
        if (page.saveBusy || page.selectedAccount === null) {
            return
        }
        var check = controller.checkReconciliation(page.selectedAccount.id)
        if (check.available && !check.endOk) {
            reconciliationDialog.check = check
            reconciliationDialog.open()
            return
        }
        doSave()
    }

    function doSave() {
        page.saveBusy = true
        saveButtonResetTimer.restart()
        var accountId = page.selectedAccount !== null ? page.selectedAccount.id : -1
        var label = page.selectedAccount !== null ? page.selectedAccount.name : qsTr("(no account selected)")
        if (!controller.save(accountId, label)) {
            // Nothing was saved: stay on the review step. The status badge
            // tells why, in red (#39).
            return
        }
        // Pick up the transactions/hash write this just made (and a
        // possible new "Reconciliation Adjustment" category) — see
        // AccountController.reload()/CategoryController.reload() doc
        // comments (no C++-to-C++ coupling between these controllers).
        AccountController.reload()
        CategoryController.reload()
        page.step = 2
    }

    // ── Confidence display ──────────────────────────────────────────────────
    // There are no confidenceVariant()/confidenceText() JS helpers here any
    // more: the bucketing (danger/warning/success at 0.5/0.8) and the percent
    // rounding moved to C++ as Fmt::confidenceVariant()/confidenceText()
    // (src/utils/Formatter.h), and each row now arrives with ready-to-display
    // `confidenceText`/`confidenceVariant` strings alongside the raw
    // `confidence` — same "values arrive pre-formatted from the controller"
    // convention as every other column. The thresholds belong next to the
    // predictor that produces the numbers (see docs/category-prediction.md);
    // duplicating them here is how the two drift apart.
    //
    // `confidence <= 0` still means "not scored" — either the predictor found
    // nothing confident enough, or the user has set the category by hand —
    // never "the model is 0% sure". Fmt renders that as a neutral "—" badge.

    // ── Transaction-table column widths — resizable + persisted ────────────
    // Defaults below match the table's original hand-picked widths, except:
    // NAME was 400 (a leftover from before NAME/DESC. were split into a short
    // editable label + a view-icon popup — see the DESC. column comment
    // below), now 100 since it only ever holds a short label today; CATEGORY/
    // SUBCATEGORY were 115, widened to 160 since category/subcategory names
    // were cropping. Every header cell (see the table below) drags its own
    // width via ColumnHeaderCell.qml's handle; both the header row and each
    // data row read the same `columnWidths` properties, so they always stay
    // aligned. Persisted across app restarts via QtCore's `Settings` type
    // (https://doc.qt.io/qt-6/qml-qtcore-settings.html — the recommended
    // property-alias pattern from that page's own docs; supersedes the now-
    // deprecated Qt.labs.settings, https://doc.qt.io/qt-6/qml-qt-labs-settings-settings.html).
    QtObject {
        id: columnWidths
        property real opDate: 80
        property real name: 100
        property real desc: 44
        property real debit: 90
        property real credit: 90
        property real balance: 100
        property real category: 160
        property real subcategory: 160
        property real confidence: 90
        property real rule: 44
        property real keep: 50
    }

    Settings {
        category: "ImportTransactionsColumns"
        property alias opDateWidth: columnWidths.opDate
        property alias nameWidth: columnWidths.name
        property alias descWidth: columnWidths.desc
        property alias debitWidth: columnWidths.debit
        property alias creditWidth: columnWidths.credit
        property alias balanceWidth: columnWidths.balance
        property alias categoryWidth: columnWidths.category
        property alias subcategoryWidth: columnWidths.subcategory
        property alias confidenceWidth: columnWidths.confidence
        property alias ruleWidth: columnWidths.rule
        property alias keepWidth: columnWidths.keep
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Select a statement file")
        fileMode: FileDialog.OpenFile
        nameFilters: page.currentTemplate
            ? [page.currentTemplate.fileFilter, qsTr("All files (*)")]
            : [qsTr("All files (*)")]
        onAccepted: {
            // The button is disabled with no template, but check again (#68).
            if (!page.currentTemplate)
                return
            controller.parseFile(selectedFile, page.currentTemplate.id)
            page.goToReview()
        }
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
                title: qsTr("Import Transactions")
                onBackClicked: page.closeRequested()

                StatusBadge {
                    visible: controller.statusText.length > 0
                    text: controller.statusText
                    variant: controller.statusLevel
                }
            }

            // ── Step 1 — template + file ────────────────────────────────────────
            ChartCard {
                visible: page.step === 0
                Layout.fillWidth: true
                implicitHeight: pickCol.implicitHeight + 50
                title: qsTr("1. Choose a template and a file")

                ColumnLayout {
                    id: pickCol
                    anchors.fill: parent
                    spacing: 14

                    RowLayout {
                        spacing: 10
                        Text { Layout.preferredWidth: 90; text: qsTr("Template"); font.pixelSize: 15; color: Theme.overlay0 }
                        ComboBox {
                            id: templateBox
                            Layout.fillWidth: true
                            model: controller.templates
                            textRole: "displayName"
                            valueRole: "id"
                            onActivated: page.currentTemplateIndex = currentIndex
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Only one template is available today (a checking-account CSV export). "
                                   + "More bank/PDF templates will be added over time — each gets its own parser "
                                   + "(see ARCHITECTURE.MD § Statement Import).")
                        font.pixelSize: 13
                        color: Theme.overlay0
                    }

                    PillButton {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Choose file & Import")
                        variant: "primary"
                        Layout.preferredHeight: 32
                        enabled: page.currentTemplate !== null
                        onClicked: fileDialog.open()
                    }
                }
            }

            // ── Step 2 — review / edit ──────────────────────────────────────────
            ColumnLayout {
                visible: page.step === 1
                Layout.fillWidth: true
                spacing: 16

                // Parse failed entirely — show the log prominently, offer retry.
                ChartCard {
                    visible: controller.lastParseFailed
                    Layout.fillWidth: true
                    implicitHeight: 120
                    title: qsTr("Import failed")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("The file could not be parsed. Open the log below for details.")
                            color: Theme.red
                            font.pixelSize: 15
                        }
                        RowLayout {
                            spacing: 10
                            PillButton { text: qsTr("View log"); variant: "neutral"; Layout.preferredHeight: 28; onClicked: logPopup.open() }
                            PillButton {
                                text: qsTr("Try again")
                                variant: "neutral"
                                Layout.preferredHeight: 28
                                onClicked: { controller.reset(); page.step = 0 }
                            }
                        }
                    }
                }

                // Statement header info + account matching
                ChartCard {
                    visible: !controller.lastParseFailed
                    Layout.fillWidth: true
                    implicitHeight: infoCol.implicitHeight + 50
                    title: qsTr("Statement — %1").arg(controller.sourceFileName)

                    ColumnLayout {
                        id: infoCol
                        anchors.fill: parent
                        spacing: 10

                        GridLayout {
                            columns: 4
                            columnSpacing: 24
                            rowSpacing: 6
                            Layout.fillWidth: true

                            Text { text: qsTr("Holder");      font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: qsTr("Account");     font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: "IBAN";        font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: qsTr("Period");      font.pixelSize: 13; color: Theme.overlay0 }

                            Text { text: controller.statementInfo.holderName;    font.pixelSize: 15; color: Theme.text }
                            Text { text: controller.statementInfo.accountName;   font.pixelSize: 15; color: Theme.text }
                            Text { text: controller.statementInfo.iban;          font.pixelSize: 15; color: Theme.text }
                            Text { text: controller.statementInfo.periodFromText + " → " + controller.statementInfo.periodToText; font.pixelSize: 15; color: Theme.text }

                            Text { text: qsTr("Balance start"); font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: qsTr("Balance end");   font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: qsTr("Rows parsed");   font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: qsTr("Issues");        font.pixelSize: 13; color: Theme.overlay0 }

                            Text { text: controller.statementInfo.balanceStartText + " (" + controller.statementInfo.balanceStartDateText + ")"; font.pixelSize: 15; color: Theme.text }
                            Text { text: controller.statementInfo.balanceEndText + " (" + controller.statementInfo.balanceEndDateText + ")"; font.pixelSize: 15; color: Theme.text }
                            Text { text: controller.statementInfo.transactionCount; font.pixelSize: 15; color: Theme.text }
                            StatusBadge {
                                pillHeight: 22
                                variant: controller.errorCount > 0 ? "danger" : (controller.warningCount > 0 ? "warning" : "success")
                                text: (controller.errorCount + controller.warningCount) === 0
                                      ? qsTr("No issues") : qsTr("%1 error(s), %2 warning(s)").arg(controller.errorCount).arg(controller.warningCount)
                                TapHandler { onTapped: logPopup.open() }
                            }
                        }

                        // The account tag (#203): one byte of a slow hash of the IBAN,
                        // used by matchAccountIndex() (see utils/AccountHashing.h and
                        // § Account tags in ARCHITECTURE.MD). Many bank accounts have
                        // the same tag, so it tells nothing about the IBAN.
                        RowLayout {
                            spacing: 24
                            Text { text: qsTr("BIC:"); font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: controller.statementInfo.bic; font.pixelSize: 15; color: Theme.text }
                            Text { text: qsTr("Account tag:"); font.pixelSize: 13; color: Theme.overlay0 }
                            Text {
                                text: controller.statementInfo.accountHash
                                font.pixelSize: 14
                                font.family: Fonts.monospace
                                color: Theme.blue
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.surface0 }

                        RowLayout {
                            spacing: 10
                            Text { text: qsTr("Target account"); font.pixelSize: 15; color: Theme.overlay0 }
                            ComboBox {
                                id: accountBox
                                Layout.preferredWidth: 260
                                model: AccountController.accounts
                                textRole: "name"
                                currentIndex: page.selectedAccountIndex
                                onActivated: page.selectAccountIndex(currentIndex)
                            }
                            Text {
                                visible: page.matchedAccountIndex >= 0 && page.matchedAccountIndex === page.selectedAccountIndex
                                text: qsTr("✓ suggested by the account tag")
                                font.pixelSize: 13
                                color: Theme.green
                            }
                            // A tag can be on two accounts (#203): Save (or "Link
                            // this account") adds it to the picked one, and the
                            // other one keeps it.
                            Text {
                                visible: page.matchedAccountIndex >= 0 && page.matchedAccountIndex !== page.selectedAccountIndex
                                text: qsTr("⚠ the tag suggests \"%1\": Save links this account too").arg(page.matchedAccountName)
                                font.pixelSize: 13
                                color: Theme.yellow
                            }
                            Text {
                                visible: page.matchedAccountIndex < 0
                                text: AccountController.accounts.length > 0
                                      ? qsTr("⚠ no automatic match — please pick the account")
                                      : qsTr("⚠ no account yet — create one")
                                font.pixelSize: 13
                                color: Theme.yellow
                            }
                            // No account fits the statement: create it here,
                            // and it becomes the target account.
                            GhostButton {
                                visible: page.matchedAccountIndex < 0
                                text: qsTr("＋ New account")
                                onClicked: newAccountDialog.openForNew()
                            }
                        }

                        // "Not linked" badge — shown whenever the currently
                        // selected account has no tag
                        // (account_hashes table), independent of whether
                        // *this* statement happened to match it. Lets the
                        // user notice and fix an unlinked account from
                        // right here instead of only discovering it on
                        // AccountsManagePage.qml (see ARCHITECTURE.MD §
                        // Account Management).
                        RowLayout {
                            visible: page.selectedAccount !== null
                            spacing: 10

                            StatusBadge {
                                visible: page.selectedAccount !== null && !page.selectedAccount.linked
                                pillHeight: 24
                                variant: "warning"
                                text: qsTr("⚠ Not linked yet — Save links it")
                            }
                            StatusBadge {
                                visible: page.selectedAccount !== null && page.selectedAccount.linked
                                pillHeight: 24
                                variant: "success"
                                text: qsTr("✓ Linked")
                            }

                            // Offer to link this statement's fingerprint to
                            // the selected account right here, when it has
                            // one to give and the account doesn't have it
                            // yet — saves a trip to AccountsManagePage.qml.
                            PillButton {
                                visible: controller.statementInfo.accountHashRaw.length > 0
                                    && page.selectedAccount !== null
                                    && !AccountController.accountHasHash(page.selectedAccountIndex, controller.statementInfo.accountHashRaw)
                                text: qsTr("Link this account")
                                variant: "primary"
                                Layout.preferredHeight: 24
                                fontSize: 13
                                onClicked: {
                                    if (AccountController.linkAccountHash(page.selectedAccountIndex,
                                                                          controller.statementInfo.accountHashRaw)) {
                                        page.readMatches()
                                    }
                                }
                            }
                        }
                    }
                }

                // Transaction table
                ChartCard {
                    visible: !controller.lastParseFailed
                    Layout.fillWidth: true
                    implicitHeight: txCol.implicitHeight + 50
                    title: qsTr("Transactions")

                    ColumnLayout {
                        id: txCol
                        anchors.fill: parent
                        spacing: 0

                        // What the category predictor managed to fill in for this
                        // statement, in its own words — `summary` is assembled in C++
                        // (SnapshotImportController::predictionInfo()) so the counts and
                        // the badge colors can't tell two different stories. On a fresh
                        // database it explains why everything is blank instead of looking
                        // broken. See docs/category-prediction.md. While the worker
                        // thread trains and scores (#52), an indicator says so.
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 8
                            spacing: 8
                            BusyIndicator {
                                running: controller.predicting
                                visible: running
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                            }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: controller.predicting ? qsTr("The app calculates the category suggestions…")
                                                            : controller.predictionInfo.summary
                                font.pixelSize: 13
                                color: Theme.overlay0
                            }
                        }

                        // The columns need more width than the card has at the default window
                        // size (1280 px), so the header and the rows scroll horizontally (#60).
                        // The rows fill the card when it is wider than the columns.
                        // TableScrollView: a vertical wheel over the table scrolls the page.
                        TableScrollView {
                            id: txScroll
                            Layout.fillWidth: true
                            // The layout gives the width, and the table scrolls in it. The
                            // default preferred width is the implicit width, which comes from the
                            // table width: that is a binding loop.
                            Layout.preferredWidth: 0
                            Layout.preferredHeight: txTable.implicitHeight
                                + (txScroll.ScrollBar.horizontal.visible ? txScroll.ScrollBar.horizontal.height : 0)
                            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                            // Always shown when the table is wider than the card: with AsNeeded, the
                            // Basic style shows the bar only while the pointer is on it, and nothing
                            // tells that the table scrolls.
                            ScrollBar.horizontal.policy: txTable.columnsWidth > txScroll.availableWidth
                                ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                            contentWidth: txTable.width
                            contentHeight: txTable.implicitHeight

                            ColumnLayout {
                                id: txTable
                                // The width of the card, or the width of the columns when it is more:
                                // the 11 columns, 11 spacings of 8 between the 12 items of a row
                                // (with the spacer), and the 10 + 10 margins of the row. It is
                                // not read from the header RowLayout: its implicit width changes
                                // with its width, and that is a binding loop.
                                readonly property real columnsWidth: columnWidths.opDate + columnWidths.name
                                    + columnWidths.desc + columnWidths.debit + columnWidths.credit
                                    + columnWidths.balance + columnWidths.category + columnWidths.subcategory
                                    + columnWidths.confidence + columnWidths.rule + columnWidths.keep
                                    + 11 * 8 + 20
                                // Delayed: the width changes the implicit height of this layout, that
                                // height changes the layout of the card, and that layout gives
                                // availableWidth. Without the delay, that is a binding loop.
                                Binding on width { value: Math.max(txScroll.availableWidth, txTable.columnsWidth); delayed: true }
                                spacing: 0

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    color: Theme.mantle
                                    radius: 4
                                    RowLayout {
                                        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                        spacing: 8

                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.opDate
                                            text: qsTr("OP. DATE")
                                            minWidth: 60
                                            onWidthChangeRequested: (w) => columnWidths.opDate = w
                                        }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.name
                                            text: qsTr("NAME")
                                            minWidth: 60
                                            onWidthChangeRequested: (w) => columnWidths.name = w
                                        }
                                        // Narrow column — rows only show a view-icon here (see the
                                        // "Full description" Popup below), the fillWidth spacer after
                                        // it takes up the space the old fillWidth DESCRIPTION text
                                        // column used to, so DEBIT/CREDIT/… stay pinned to the right.
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.desc
                                            text: qsTr("DESC.")
                                            minWidth: 32
                                            onWidthChangeRequested: (w) => columnWidths.desc = w
                                        }
                                        Item { Layout.fillWidth: true }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.debit
                                            text: qsTr("DEBIT")
                                            horizontalAlignment: Text.AlignRight
                                            minWidth: 50
                                            onWidthChangeRequested: (w) => columnWidths.debit = w
                                        }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.credit
                                            text: qsTr("CREDIT")
                                            horizontalAlignment: Text.AlignRight
                                            minWidth: 50
                                            onWidthChangeRequested: (w) => columnWidths.credit = w
                                        }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.balance
                                            text: qsTr("BALANCE")
                                            horizontalAlignment: Text.AlignRight
                                            minWidth: 60
                                            onWidthChangeRequested: (w) => columnWidths.balance = w
                                        }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.category
                                            text: qsTr("CATEGORY")
                                            minWidth: 70
                                            onWidthChangeRequested: (w) => columnWidths.category = w
                                        }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.subcategory
                                            text: qsTr("SUBCATEGORY")
                                            minWidth: 70
                                            onWidthChangeRequested: (w) => columnWidths.subcategory = w
                                        }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.confidence
                                            text: qsTr("CONFIDENCE")
                                            horizontalAlignment: Text.AlignHCenter
                                            minWidth: 60
                                            onWidthChangeRequested: (w) => columnWidths.confidence = w
                                        }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.rule
                                            text: qsTr("RULE")
                                            horizontalAlignment: Text.AlignHCenter
                                            minWidth: 40
                                            onWidthChangeRequested: (w) => columnWidths.rule = w
                                        }
                                        ColumnHeaderCell {
                                            Layout.preferredWidth: columnWidths.keep
                                            text: qsTr("KEEP")
                                            horizontalAlignment: Text.AlignRight
                                            minWidth: 40
                                            onWidthChangeRequested: (w) => columnWidths.keep = w
                                        }
                                    }
                                }

                                Repeater {
                                    model: controller.transactionRows
                                    delegate: Rectangle {
                                        id: txRow
                                        required property var modelData
                                        required property int index
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 44
                                        color: index % 2 === 0 ? "transparent" : Theme.mantle
                                        opacity: modelData.included ? 1.0 : 0.4

                                        RowLayout {
                                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                            spacing: 8

                                            Text { Layout.preferredWidth: columnWidths.opDate; text: txRow.modelData.operationDate; font.pixelSize: 14; color: Theme.overlay0 }
                                            TextField {
                                                id: nameField
                                                objectName: "rowNameField" // see onPredictionsAboutToBeApplied
                                                Layout.preferredWidth: columnWidths.name
                                                Layout.preferredHeight: 32
                                                text: txRow.modelData.name
                                                font.pixelSize: 15
                                                color: Theme.text
                                                placeholderTextColor: Theme.surface1
                                                placeholderText: qsTr("(no name)")
                                                // Same recipe as FormRow.qml's TextField — see
                                                // ARCHITECTURE.MD § Shared Components.
                                                background: Rectangle {
                                                    color: Theme.crust
                                                    radius: 4
                                                    border.color: nameField.activeFocus ? Theme.blue : Theme.surface0
                                                }
                                                // Auto-extracted at parse time (see
                                                // extractTransactionName() in
                                                // SumeriaCcpCsvExtractionTemplate.cpp); editable here in
                                                // case the extraction picked the wrong thing.
                                                onEditingFinished: controller.setRowName(txRow.modelData.index, text)
                                            }
                                            // Full description is no longer shown inline — just an icon
                                            // that opens descriptionPopup (see below) with the full,
                                            // selectable/copyable text. Keeps the row from being
                                            // dominated by often-long multi-line bank text now that NAME
                                            // (above) carries the at-a-glance label.
                                            GhostButton {
                                                Layout.preferredWidth: columnWidths.desc
                                                text: "🔍"
                                                onClicked: {
                                                    descriptionPopup.descriptionText = txRow.modelData.description
                                                    descriptionPopup.open()
                                                }
                                            }
                                            Item { Layout.fillWidth: true }
                                            Text { Layout.preferredWidth: columnWidths.debit;   text: txRow.modelData.debitText;  font.pixelSize: 15; color: Theme.red; horizontalAlignment: Text.AlignRight }
                                            Text { Layout.preferredWidth: columnWidths.credit;  text: txRow.modelData.creditText; font.pixelSize: 15; color: Theme.green; horizontalAlignment: Text.AlignRight }
                                            Text { Layout.preferredWidth: columnWidths.balance; text: txRow.modelData.balanceText; font.pixelSize: 14; color: Theme.overlay0; horizontalAlignment: Text.AlignRight }

                                            ColorComboBox {
                                                id: categoryBox
                                                Layout.preferredWidth: columnWidths.category
                                                Layout.preferredHeight: 36
                                                items: CategoryController.categories
                                                currentName: txRow.modelData.category
                                                onNameActivated: (name) => controller.setRowCategory(txRow.modelData.index, name)
                                            }

                                            ColorComboBox {
                                                Layout.preferredWidth: columnWidths.subcategory
                                                Layout.preferredHeight: 36
                                                readonly property var subItems: {
                                                    var cat = CategoryController.categories.find(function(c) { return c.name === txRow.modelData.category })
                                                    return cat ? cat.subcategories : []
                                                }
                                                items: subItems
                                                enabled: subItems.length > 0
                                                currentName: txRow.modelData.subcategory
                                                onNameActivated: (name) => controller.setRowSubcategory(txRow.modelData.index, name)
                                            }

                                            // Pre-formatted in C++ (Fmt::confidenceVariant()/
                                            // confidenceText(), reached via the row map) — see the
                                            // "Confidence display" comment at the top of this file.
                                            // Hovering explains *why* the predictor picked this
                                            // category, which is the difference between a number the
                                            // user can act on and one they just have to trust.
                                            StatusBadge {
                                                Layout.preferredWidth: columnWidths.confidence
                                                Layout.alignment: Qt.AlignHCenter
                                                pillHeight: 24
                                                variant: txRow.modelData.confidenceVariant
                                                text: txRow.modelData.confidenceText

                                                HoverHandler { id: confidenceHover }
                                                ToolTip.text: txRow.modelData.predictionReason
                                                ToolTip.visible: confidenceHover.hovered
                                                                  && txRow.modelData.predictionReason !== ""
                                                ToolTip.delay: 300
                                            }

                                            // "Always categorize labels like this one the same way."
                                            // One click turns this row's normalized merchant key into
                                            // a permanent Data::CategoryRule and re-scores every other
                                            // row in the statement against it — the fastest-learning
                                            // path in the system (see docs/category-prediction.md).
                                            //
                                            // Deliberately NOT `enabled: canTeach`: Item.enabled would
                                            // also switch off the HoverHandler, so the "pick a category
                                            // first" tooltip — the one explaining why it looks inert —
                                            // would be the one thing the user couldn't see. It stays
                                            // clickable and dimmed instead; rememberRowAsRule() already
                                            // refuses and posts that same message to the status line.
                                            GhostButton {
                                                id: ruleButton
                                                readonly property bool canTeach: txRow.modelData.category !== ""
                                                                                  && txRow.modelData.merchantKey !== ""
                                                Layout.preferredWidth: columnWidths.rule
                                                text: "＋"
                                                opacity: ruleButton.canTeach ? 1.0 : 0.35
                                                onClicked: controller.rememberRowAsRule(txRow.modelData.index)

                                                HoverHandler { id: ruleHover }
                                                ToolTip.text: ruleButton.canTeach
                                                    ? qsTr("Always categorize “%1” as %2").arg(txRow.modelData.merchantKey)
                                                      .arg(txRow.modelData.category
                                                           + (txRow.modelData.subcategory !== ""
                                                              ? " / " + txRow.modelData.subcategory : ""))
                                                    : (txRow.modelData.category === ""
                                                       ? qsTr("Pick a category for this row first")
                                                       // A label with only generic words has no key (#40).
                                                       : qsTr("This label has no merchant name to build a rule from"))
                                                ToolTip.visible: ruleHover.hovered
                                                ToolTip.delay: 300
                                            }

                                            CheckBox {
                                                Layout.preferredWidth: columnWidths.keep
                                                checked: txRow.modelData.included
                                                onCheckedChanged: controller.setRowIncluded(txRow.modelData.index, checked)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Footer actions
                RowLayout {
                    visible: !controller.lastParseFailed
                    Layout.fillWidth: true
                    spacing: 10

                    PillButton { text: qsTr("View log"); variant: "neutral"; onClicked: logPopup.open() }
                    PillButton {
                        text: qsTr("Import another file")
                        variant: "neutral"
                        onClicked: { controller.reset(); page.step = 0 }
                    }
                    Item { Layout.fillWidth: true }
                    PillButton {
                        text: page.saveBusy ? qsTr("Saving…") : qsTr("Save")
                        variant: "primary"
                        // The suggestions come from a worker thread (#52): wait for them.
                        enabled: !page.saveBusy && page.selectedAccount !== null && !controller.predicting
                        onClicked: page.requestSave()
                    }
                }
            }

            // ── Step 3 — import report ──────────────────────────────────────────
            // Shown right after a successful Save (see doSave() above) — a third
            // step inside this same Item/controller rather than a separate
            // overlay page, so SnapshotImportController's state (and the report
            // it just built) survives the transition; a separate Loader-based
            // overlay would destroy/recreate the controller (see ARCHITECTURE.MD
            // § Statement Import — Import Report for why).
            ColumnLayout {
                id: reportStep
                visible: page.step === 2
                Layout.fillWidth: true
                spacing: 16

                readonly property var report: controller.lastImportReport
                readonly property var reconciliation: reportStep.report.reconciliation ? reportStep.report.reconciliation : ({})
                readonly property var byCategory: reportStep.report.byCategory ? reportStep.report.byCategory : []

                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: reportSummaryCol.implicitHeight + 50
                    title: qsTr("Import report — %1").arg(reportStep.report.accountLabel ? reportStep.report.accountLabel : "")

                    ColumnLayout {
                        id: reportSummaryCol
                        anchors.fill: parent
                        spacing: 14

                        GridLayout {
                            columns: 4
                            columnSpacing: 24
                            rowSpacing: 6
                            Layout.fillWidth: true

                            Text { text: qsTr("Transactions saved"); font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: qsTr("Total income");       font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: qsTr("Total expenses");     font.pixelSize: 13; color: Theme.overlay0 }
                            Text { text: qsTr("Net change");         font.pixelSize: 13; color: Theme.overlay0 }

                            Text { text: reportStep.report.transactionCount !== undefined ? reportStep.report.transactionCount : 0; font.pixelSize: 18; font.bold: true; color: Theme.text }
                            Text { text: reportStep.report.totalIncomeText ? reportStep.report.totalIncomeText : "—"; font.pixelSize: 18; font.bold: true; color: Theme.green }
                            Text { text: reportStep.report.totalExpenseText ? reportStep.report.totalExpenseText : "—"; font.pixelSize: 18; font.bold: true; color: Theme.red }
                            Text { text: reportStep.report.netChangeText ? reportStep.report.netChangeText : "—"; font.pixelSize: 18; font.bold: true; color: Theme.text }
                        }

                        // Rows that this account already had (a re-import, or a
                        // statement that overlaps an earlier one): save() skips
                        // them, and this line tells the user why the count is lower.
                        Text {
                            visible: (reportStep.report.alreadySavedCount || 0) > 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("%1 transaction(s) of this statement were already saved, so they were not added again.")
                                .arg(reportStep.report.alreadySavedCount || 0)
                            font.pixelSize: 13
                            color: Theme.yellow
                        }

                        // Of those, rows with the bank reference and the amount of
                        // a saved transaction but another operation date: the bank
                        // may reuse its references. The link opens the issue.
                        Text {
                            visible: (reportStep.report.reusedReferenceCount || 0) > 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            textFormat: Text.StyledText
                            text: qsTr("⚠ %1 of these rows have the bank reference and the amount of a saved transaction,"
                                       + " but another operation date, so they were not saved. If they are different"
                                       + " transactions, the bank reuses its references: see <a href=\"%2\">this issue</a>.")
                                .arg(reportStep.report.reusedReferenceCount || 0)
                                .arg(reportStep.report.reusedReferenceUrl || "")
                            font.pixelSize: 13
                            color: Theme.peach
                            linkColor: Theme.blue
                            onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                        }
                    }
                }

                // ── Reconciliation results ──────────────────────────────────────
                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: reconCol.implicitHeight + 50
                    title: qsTr("Reconciliation")

                    ColumnLayout {
                        id: reconCol
                        anchors.fill: parent
                        spacing: 10

                        RowLayout {
                            spacing: 10
                            StatusBadge {
                                pillHeight: 22
                                variant: !reportStep.reconciliation.hasStartReference ? "neutral"
                                    : (reportStep.reconciliation.startOk ? "success" : "warning")
                                // A start line that could not be read (#44): not checked.
                                text: reportStep.reconciliation.hasDeclaredStart === false ? qsTr("Start balance — not in the file")
                                    : !reportStep.reconciliation.hasStartReference ? qsTr("Start balance — no prior record")
                                    : (reportStep.reconciliation.startOk ? qsTr("Start balance OK") : qsTr("Start balance mismatch"))
                            }
                            Text {
                                visible: reportStep.reconciliation.hasStartReference === true
                                text: qsTr("declared %1 vs. %2 (%3 on %4) — diff %5")
                                    .arg(reportStep.reconciliation.startDeclaredText || "—")
                                    .arg(reportStep.reconciliation.startExpectedText || "—")
                                    .arg(reportStep.reconciliation.startSourceLabel || "")
                                    .arg(reportStep.reconciliation.startSourceDateText || "—")
                                    .arg(reportStep.reconciliation.startDiffText || "—")
                                font.pixelSize: 13
                                color: Theme.overlay0
                            }
                        }

                        RowLayout {
                            spacing: 10
                            StatusBadge {
                                pillHeight: 22
                                // A balance line that could not be read (#44): not checked.
                                variant: reportStep.reconciliation.endAvailable === false ? "neutral"
                                    : (reportStep.reconciliation.endOk ? "success" : "danger")
                                text: reportStep.reconciliation.endAvailable === false ? qsTr("End balance — not checked")
                                    : (reportStep.reconciliation.endOk ? qsTr("End balance OK") : qsTr("End balance mismatch"))
                            }
                            Text {
                                visible: reportStep.reconciliation.endAvailable !== false
                                text: qsTr("computed %1 vs. declared %2 — diff %3")
                                    .arg(reportStep.reconciliation.endComputedText || "—")
                                    .arg(reportStep.reconciliation.endDeclaredText || "—")
                                    .arg(reportStep.reconciliation.endDiffText || "—")
                                font.pixelSize: 13
                                color: Theme.overlay0
                            }
                        }
                    }
                }

                // ── Category / subcategory breakdown — pie chart + legend ───────
                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: breakdownCol.implicitHeight + 50
                    title: qsTr("By category — click a slice")

                    ColumnLayout {
                        id: breakdownCol
                        anchors.fill: parent
                        spacing: 10

                        EmptyState {
                            visible: reportStep.byCategory.length === 0
                            message: qsTr("No transactions were saved.")
                        }

                        // Pie chart: slice size is each category's spending/income
                        // *magnitude* (Math.abs of the net total) — a pie of signed
                        // net amounts (income positive, expenses negative) isn't a
                        // meaningful "share of the whole", so this shows "how much
                        // of the money that moved went through each category"
                        // regardless of direction; the exact signed totals are still
                        // shown in the legend rows below. Same GraphsView/PieSeries
                        // recipe as CashFlowPage.qml's category donut (verified
                        // against that page, not invented — see ARCHITECTURE.MD
                        // header note on checking QtGraphsExamples/doc.qt.io first).
                        GraphsView {
                            visible: reportStep.byCategory.length > 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark }

                            PieSeries {
                                id: categoryPie
                                // Without it the renderer emits no clicked signal (default false).
                                selectable: true
                                pieSize: 0.75
                                holeSize: 0.42

                                function rebuild() {
                                    clear()
                                    pieSliceInfo.text = qsTr("Click a slice for details")
                                    var cats = reportStep.byCategory
                                    for (var i = 0; i < cats.length; i++) {
                                        var magnitude = Math.abs(cats[i].totalRaw)
                                        if (magnitude <= 0)
                                            continue
                                        var slice = append(cats[i].category, magnitude)
                                        slice.color = CategoryController.categoryColor(cats[i].category) || Theme.overlay0
                                        // Without it, the slice gets the outline colour of the theme (#71).
                                        slice.borderColor = slice.color
                                        slice.labelVisible = true
                                    }
                                }
                                Component.onCompleted: rebuild()

                                onClicked: (slice) => {
                                    pieSliceInfo.text = slice.label + "  —  "
                                        + slice.value.toLocaleString(Qt.locale("fr-FR"), 'f', 0)
                                        + " €  (" + Math.round(slice.percentage * 100) + " %)"
                                }
                            }

                            Connections {
                                target: reportStep
                                function onByCategoryChanged() { categoryPie.rebuild() }
                            }
                        }
                        Text {
                            id: pieSliceInfo
                            Layout.alignment: Qt.AlignHCenter
                            visible: reportStep.byCategory.length > 0
                            text: qsTr("Click a slice for details")
                            font.pixelSize: 14
                            color: Theme.overlay0
                        }

                        Rectangle {
                            visible: reportStep.byCategory.length > 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.surface0
                        }

                        Repeater {
                            model: reportStep.byCategory
                            delegate: ColumnLayout {
                                id: catRow
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 4

                                RowLayout {
                                    spacing: 10
                                    Rectangle {
                                        Layout.preferredWidth: catBadge.implicitWidth + 16
                                        Layout.preferredHeight: 22
                                        radius: 11
                                        color: Qt.rgba(1, 1, 1, 0.08)
                                        border.color: CategoryController.categoryColor(catRow.modelData.category) || Theme.overlay0
                                        Text {
                                            id: catBadge
                                            anchors.centerIn: parent
                                            text: catRow.modelData.category
                                            font.pixelSize: 13
                                            color: CategoryController.categoryColor(catRow.modelData.category) || Theme.overlay0
                                        }
                                    }
                                    Text {
                                        text: catRow.modelData.totalText
                                        font.pixelSize: 15
                                        font.bold: true
                                        color: catRow.modelData.totalRaw >= 0 ? Theme.green : Theme.red
                                    }
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 20
                                    spacing: 6

                                    Repeater {
                                        model: catRow.modelData.subcategories
                                        delegate: Rectangle {
                                            id: subChip
                                            required property var modelData
                                            width: subChipRow.implicitWidth + 14
                                            height: 20
                                            radius: 10
                                            color: Theme.mantle
                                            border.color: CategoryController.subcategoryColor(catRow.modelData.category, subChip.modelData.subcategory) || Theme.surface0

                                            RowLayout {
                                                id: subChipRow
                                                anchors.centerIn: parent
                                                spacing: 6
                                                Text {
                                                    text: subChip.modelData.subcategory + " · " + subChip.modelData.totalText
                                                    font.pixelSize: 11
                                                    color: Theme.subtext0
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    PillButton {
                        text: qsTr("Import another file")
                        variant: "primary"
                        onClicked: { controller.reset(); page.step = 0 }
                    }
                    Item { Layout.fillWidth: true }
                    // The page stays loaded after it closes (Main.qml, #64): reset it,
                    // so that the next import starts at step 1.
                    PillButton {
                        text: qsTr("Done")
                        variant: "neutral"
                        onClicked: { controller.reset(); page.step = 0; page.closeRequested() }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }

    // ── Log / console popup ─────────────────────────────────────────────────────
    Popup {
        id: logPopup
        anchors.centerIn: parent
        width: 640
        height: 420
        padding: 16
        modal: true

        background: Rectangle {
            color: Theme.mantle
            border.color: Theme.surface1
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Import log"); font.pixelSize: 18; font.bold: true; color: Theme.text }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("%1 error(s) · %2 warning(s)").arg(controller.errorCount).arg(controller.warningCount)
                    font.pixelSize: 14
                    color: Theme.overlay0
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: logPopup.width - 32
                    spacing: 6

                    EmptyState {
                        visible: controller.logEntries.length === 0
                        message: qsTr("No log entries.")
                    }

                    Repeater {
                        model: controller.logEntries
                        delegate: ColumnLayout {
                            id: logRow
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                spacing: 8
                                StatusBadge {
                                    pillHeight: 18
                                    // "Info" (anything not Error/Warning) reuses the
                                    // "success" (green) styling — same semantic this
                                    // level already used everywhere else (a positive/
                                    // neutral log signal), now a full pill instead of
                                    // the old grey-bg/green-text hybrid.
                                    variant: logRow.modelData.level === "Error" ? "danger"
                                           : logRow.modelData.level === "Warning" ? "warning" : "success"
                                    // The level is a key ("Error", "Warning", "Info"): the badge shows its translation.
                                    text: logRow.modelData.level === "Error" ? qsTr("Error")
                                        : logRow.modelData.level === "Warning" ? qsTr("Warning") : qsTr("Info")
                                }
                                Text {
                                    text: qsTr("L%1").arg(logRow.modelData.line)
                                    font.pixelSize: 13
                                    color: Theme.overlay0
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: logRow.modelData.message
                                    font.pixelSize: 15
                                    color: Theme.text
                                    wrapMode: Text.WordWrap
                                }
                            }
                            Text {
                                visible: logRow.modelData.rawLine.length > 0
                                Layout.fillWidth: true
                                Layout.leftMargin: 70
                                text: logRow.modelData.rawLine
                                font.pixelSize: 13
                                font.family: Fonts.monospace
                                color: Theme.surface1
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                PillButton { text: qsTr("Close"); variant: "neutral"; onClicked: logPopup.close() }
            }
        }
    }

    // ── Full-description popup ──────────────────────────────────────────────────
    // Opened by the 🔍 icon in the transaction table (replaced the old inline
    // DESCRIPTION text column — see the table above). One shared instance for
    // every row; the row's onClicked sets descriptionText right before open().
    // Text is selectable (TextEdit.selectByMouse) so the raw bank description
    // (refs included) can be copy-pasted elsewhere.
    Popup {
        id: descriptionPopup
        anchors.centerIn: parent
        width: 480
        height: 280
        padding: 16
        modal: true

        property string descriptionText: ""

        background: Rectangle {
            color: Theme.mantle
            border.color: Theme.surface1
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text { text: qsTr("Full description"); font.pixelSize: 18; font.bold: true; color: Theme.text }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextEdit {
                    width: descriptionPopup.width - 32
                    text: descriptionPopup.descriptionText
                    readOnly: true
                    selectByMouse: true
                    persistentSelection: true
                    wrapMode: TextEdit.WordWrap
                    font.pixelSize: 15
                    color: Theme.text
                    selectionColor: Theme.blue
                    selectedTextColor: Theme.crust
                }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                PillButton { text: qsTr("Close"); variant: "neutral"; onClicked: descriptionPopup.close() }
            }
        }
    }

    // ── Reconciliation mismatch dialog ────────────────────────────────────────
    // Opened by requestSave() (see above) when checkReconciliation()'s check 2
    // (declared start + kept rows vs. declared end) fails — the "tell the user,
    // and ask what he wants to do" step from the product request. Two ways
    // forward, neither of which saves automatically: "Review kept transactions"
    // just closes the dialog so the user can toggle KEEP checkboxes in the
    // table and hit Save again; "Add adjustment transaction" appends one
    // synthetic row for the exact gap (see
    // SnapshotImportController::addReconciliationAdjustment()) tagged with the
    // "Reconciliation Adjustment" category, then also just closes — the user
    // reviews the new row and clicks Save again themselves rather than this
    // dialog re-triggering the save on their behalf.
    Popup {
        id: reconciliationDialog
        anchors.centerIn: parent
        width: 480
        padding: 16
        modal: true

        property var check: ({})

        background: Rectangle {
            color: Theme.mantle
            border.color: Theme.red
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text { text: qsTr("Balance doesn't add up"); font.pixelSize: 18; font.bold: true; color: Theme.text }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("The statement declares an ending balance of %1, but the declared starting balance plus "
                           + "every currently-kept transaction only adds up to %2 — a difference of %3.")
                    .arg(reconciliationDialog.check.endDeclaredText || "—")
                    .arg(reconciliationDialog.check.endComputedText || "—")
                    .arg(reconciliationDialog.check.endDiffText || "—")
                font.pixelSize: 14
                color: Theme.text
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Either a kept transaction was unchecked by mistake, or a real amount is missing. "
                           + "Review the KEEP column, or add a matching adjustment transaction (category "
                           + "\"Reconciliation Adjustment\") to close the gap.")
                font.pixelSize: 13
                color: Theme.overlay0
            }

            RowLayout {
                Layout.topMargin: 6
                Layout.fillWidth: true
                spacing: 10
                PillButton {
                    text: qsTr("Review kept transactions")
                    variant: "neutral"
                    onClicked: reconciliationDialog.close()
                }
                Item { Layout.fillWidth: true }
                PillButton {
                    text: qsTr("Add adjustment transaction")
                    variant: "danger"
                    onClicked: {
                        var accountId = page.selectedAccount !== null ? page.selectedAccount.id : -1
                        controller.addReconciliationAdjustment(accountId)
                        CategoryController.reload()
                        reconciliationDialog.close()
                    }
                }
            }
        }
    }

    // ── New account from the review ──────────────────────────────────────────
    // The same form as Manage Accounts. The new account becomes the target
    // account; Save links the tag of the statement to it, as for any account.
    AccountFormDialog {
        id: newAccountDialog
        onAccountAdded: (index) => page.selectAccountIndex(index)
    }
}
