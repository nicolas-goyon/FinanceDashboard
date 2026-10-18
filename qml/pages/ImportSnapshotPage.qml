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

    readonly property var selectedAccount:
        page.selectedAccountIndex >= 0 && page.selectedAccountIndex < AccountController.accounts.length
            ? AccountController.accounts[page.selectedAccountIndex] : null

    function goToReview() {
        matchedAccountIndex = controller.matchAccountIndex(AccountController.accounts)
        selectedAccountIndex = matchedAccountIndex
        page.step = 1
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
        var label = page.selectedAccount !== null ? page.selectedAccount.name : "(no account selected)"
        controller.save(accountId, label)
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
        title: "Select a statement file"
        fileMode: FileDialog.OpenFile
        nameFilters: page.currentTemplate
            ? [page.currentTemplate.fileFilter, "All files (*)"]
            : ["All files (*)"]
        onAccepted: {
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
                title: "Import Transactions"
                onBackClicked: page.closeRequested()

                StatusBadge {
                    visible: controller.statusText.length > 0
                    text: controller.statusText
                    variant: controller.lastParseFailed ? "danger" : "success"
                }
            }

            // ── Step 1 — template + file ────────────────────────────────────────
            ChartCard {
                visible: page.step === 0
                Layout.fillWidth: true
                implicitHeight: pickCol.implicitHeight + 50
                title: "1. Choose a template and a file"

                ColumnLayout {
                    id: pickCol
                    anchors.fill: parent
                    spacing: 14

                    RowLayout {
                        spacing: 10
                        Text { Layout.preferredWidth: 90; text: "Template"; font.pixelSize: 15; color: "#6c7086" }
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
                        text: "Only one template is available today (a checking-account CSV export). "
                            + "More bank/PDF templates will be added over time — each gets its own parser "
                            + "(see ARCHITECTURE.MD § Statement Import)."
                        font.pixelSize: 13
                        color: "#6c7086"
                    }

                    PillButton {
                        Layout.alignment: Qt.AlignRight
                        text: "Choose file & Import"
                        variant: "primary"
                        Layout.preferredHeight: 32
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
                    title: "Import failed"

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "The file could not be parsed. Open the log below for details."
                            color: "#f38ba8"
                            font.pixelSize: 15
                        }
                        RowLayout {
                            spacing: 10
                            PillButton { text: "View log"; variant: "neutral"; Layout.preferredHeight: 28; onClicked: logPopup.open() }
                            PillButton {
                                text: "Try again"
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
                    title: "Statement — " + controller.sourceFileName

                    ColumnLayout {
                        id: infoCol
                        anchors.fill: parent
                        spacing: 10

                        GridLayout {
                            columns: 4
                            columnSpacing: 24
                            rowSpacing: 6
                            Layout.fillWidth: true

                            Text { text: "Holder";      font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "Account";     font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "IBAN";        font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "Period";      font.pixelSize: 13; color: "#6c7086" }

                            Text { text: controller.statementInfo.holderName;    font.pixelSize: 15; color: "#cdd6f4" }
                            Text { text: controller.statementInfo.accountName;   font.pixelSize: 15; color: "#cdd6f4" }
                            Text { text: controller.statementInfo.iban;          font.pixelSize: 15; color: "#cdd6f4" }
                            Text { text: controller.statementInfo.periodFromText + " → " + controller.statementInfo.periodToText; font.pixelSize: 15; color: "#cdd6f4" }

                            Text { text: "Balance start"; font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "Balance end";   font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "Rows parsed";   font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "Issues";        font.pixelSize: 13; color: "#6c7086" }

                            Text { text: controller.statementInfo.balanceStartText + " (" + controller.statementInfo.balanceStartDateText + ")"; font.pixelSize: 15; color: "#cdd6f4" }
                            Text { text: controller.statementInfo.balanceEndText + " (" + controller.statementInfo.balanceEndDateText + ")"; font.pixelSize: 15; color: "#cdd6f4" }
                            Text { text: controller.statementInfo.transactionCount; font.pixelSize: 15; color: "#cdd6f4" }
                            StatusBadge {
                                pillHeight: 22
                                variant: controller.errorCount > 0 ? "danger" : (controller.warningCount > 0 ? "warning" : "success")
                                text: (controller.errorCount + controller.warningCount) === 0
                                      ? "No issues" : (controller.errorCount + " error(s), " + controller.warningCount + " warning(s)")
                                TapHandler { onTapped: logPopup.open() }
                            }
                        }

                        // Account-hash fingerprint — HMAC-SHA256(IBAN, key=BIC), used by
                        // matchAccountIndex() instead of comparing raw IBAN/BIC (see
                        // SnapshotImportController::accountHash() / § Statement Import in
                        // ARCHITECTURE.MD). Shown here so the match is auditable.
                        RowLayout {
                            spacing: 24
                            Text { text: "BIC:"; font.pixelSize: 13; color: "#6c7086" }
                            Text { text: controller.statementInfo.bic; font.pixelSize: 15; color: "#cdd6f4" }
                            Text { text: "Account hash:"; font.pixelSize: 13; color: "#6c7086" }
                            Text {
                                text: controller.statementInfo.accountHash
                                font.pixelSize: 14
                                font.family: "monospace"
                                color: "#89b4fa"
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#313244" }

                        RowLayout {
                            spacing: 10
                            Text { text: "Target account"; font.pixelSize: 15; color: "#6c7086" }
                            ComboBox {
                                id: accountBox
                                Layout.preferredWidth: 260
                                model: AccountController.accounts
                                textRole: "name"
                                currentIndex: page.selectedAccountIndex
                                onActivated: page.selectedAccountIndex = currentIndex
                            }
                            Text {
                                visible: page.matchedAccountIndex >= 0
                                text: "✓ matched by account hash"
                                font.pixelSize: 13
                                color: "#a6e3a1"
                            }
                            Text {
                                visible: page.matchedAccountIndex < 0
                                text: "⚠ no automatic match — please pick the account"
                                font.pixelSize: 13
                                color: "#f9e2af"
                            }
                        }

                        // "Not linked" badge — shown whenever the currently
                        // selected account has zero stored hashes
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
                                text: "⚠ Not linked — no hash set up for this account"
                            }
                            StatusBadge {
                                visible: page.selectedAccount !== null && page.selectedAccount.linked
                                pillHeight: 24
                                variant: "success"
                                // Guard against null even though `visible` above already
                                // excludes this case at display time — bindings in QML are
                                // still evaluated when their dependencies change regardless
                                // of the item's own `visible` value (it's not a guard), so
                                // `page.selectedAccount.hashCount` alone would throw
                                // "Cannot read property 'hashCount' of null" the moment
                                // `page.selectedAccount` becomes null (e.g. AccountController
                                // reloads and `selectedAccountIndex` momentarily points past
                                // the end) even while this badge itself stays invisible.
                                text: page.selectedAccount !== null
                                    ? "✓ Linked (" + page.selectedAccount.hashCount + " hash"
                                      + (page.selectedAccount.hashCount === 1 ? "" : "es") + ")"
                                    : ""
                            }

                            // Offer to link this statement's fingerprint to
                            // the selected account right here, when it has
                            // one to give and the account doesn't have it
                            // yet — saves a trip to AccountsManagePage.qml.
                            PillButton {
                                visible: controller.statementInfo.accountHashRaw.length > 0
                                    && page.selectedAccount !== null
                                    && !AccountController.accountHasHash(page.selectedAccountIndex, controller.statementInfo.accountHashRaw)
                                text: "Link this account"
                                variant: "primary"
                                Layout.preferredHeight: 24
                                fontSize: 13
                                onClicked: AccountController.linkAccountHash(
                                    page.selectedAccountIndex, controller.statementInfo.accountHashRaw)
                            }
                        }
                    }
                }

                // Transaction table
                ChartCard {
                    visible: !controller.lastParseFailed
                    Layout.fillWidth: true
                    implicitHeight: txCol.implicitHeight + 50
                    title: "Transactions"

                    ColumnLayout {
                        id: txCol
                        anchors.fill: parent
                        spacing: 0

                        // What the category predictor managed to fill in for this
                        // statement, in its own words — `summary` is assembled in C++
                        // (SnapshotImportController::predictionInfo()) so the counts and
                        // the badge colors can't tell two different stories. On a fresh
                        // database it explains why everything is blank instead of looking
                        // broken. See docs/category-prediction.md.
                        Text {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 8
                            wrapMode: Text.WordWrap
                            text: controller.predictionInfo.summary
                            font.pixelSize: 13
                            color: "#6c7086"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            color: "#181825"
                            radius: 4
                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                spacing: 8

                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.opDate
                                    text: "OP. DATE"
                                    minWidth: 60
                                    onWidthChangeRequested: (w) => columnWidths.opDate = w
                                }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.name
                                    text: "NAME"
                                    minWidth: 60
                                    onWidthChangeRequested: (w) => columnWidths.name = w
                                }
                                // Narrow column — rows only show a view-icon here (see the
                                // "Full description" Popup below), the fillWidth spacer after
                                // it takes up the space the old fillWidth DESCRIPTION text
                                // column used to, so DEBIT/CREDIT/… stay pinned to the right.
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.desc
                                    text: "DESC."
                                    minWidth: 32
                                    onWidthChangeRequested: (w) => columnWidths.desc = w
                                }
                                Item { Layout.fillWidth: true }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.debit
                                    text: "DEBIT"
                                    horizontalAlignment: Text.AlignRight
                                    minWidth: 50
                                    onWidthChangeRequested: (w) => columnWidths.debit = w
                                }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.credit
                                    text: "CREDIT"
                                    horizontalAlignment: Text.AlignRight
                                    minWidth: 50
                                    onWidthChangeRequested: (w) => columnWidths.credit = w
                                }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.balance
                                    text: "BALANCE"
                                    horizontalAlignment: Text.AlignRight
                                    minWidth: 60
                                    onWidthChangeRequested: (w) => columnWidths.balance = w
                                }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.category
                                    text: "CATEGORY"
                                    minWidth: 70
                                    onWidthChangeRequested: (w) => columnWidths.category = w
                                }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.subcategory
                                    text: "SUBCATEGORY"
                                    minWidth: 70
                                    onWidthChangeRequested: (w) => columnWidths.subcategory = w
                                }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.confidence
                                    text: "CONFIDENCE"
                                    horizontalAlignment: Text.AlignHCenter
                                    minWidth: 60
                                    onWidthChangeRequested: (w) => columnWidths.confidence = w
                                }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.rule
                                    text: "RULE"
                                    horizontalAlignment: Text.AlignHCenter
                                    minWidth: 40
                                    onWidthChangeRequested: (w) => columnWidths.rule = w
                                }
                                ColumnHeaderCell {
                                    Layout.preferredWidth: columnWidths.keep
                                    text: "KEEP"
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
                                color: index % 2 === 0 ? "transparent" : "#181825"
                                opacity: modelData.included ? 1.0 : 0.4

                                RowLayout {
                                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                    spacing: 8

                                    Text { Layout.preferredWidth: columnWidths.opDate; text: txRow.modelData.operationDate; font.pixelSize: 14; color: "#6c7086" }
                                    TextField {
                                        id: nameField
                                        Layout.preferredWidth: columnWidths.name
                                        Layout.preferredHeight: 32
                                        text: txRow.modelData.name
                                        font.pixelSize: 15
                                        color: "#cdd6f4"
                                        placeholderTextColor: "#45475a"
                                        placeholderText: "(no name)"
                                        // Same recipe as FormRow.qml's TextField — see
                                        // ARCHITECTURE.MD § Shared Components.
                                        background: Rectangle {
                                            color: "#11111b"
                                            radius: 4
                                            border.color: nameField.activeFocus ? "#89b4fa" : "#313244"
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
                                    Text { Layout.preferredWidth: columnWidths.debit;   text: txRow.modelData.debitText;  font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: columnWidths.credit;  text: txRow.modelData.creditText; font.pixelSize: 15; color: "#a6e3a1"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: columnWidths.balance; text: txRow.modelData.balanceText; font.pixelSize: 14; color: "#6c7086"; horizontalAlignment: Text.AlignRight }

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
                                            ? "Always categorize “" + txRow.modelData.merchantKey + "” as "
                                              + txRow.modelData.category
                                              + (txRow.modelData.subcategory !== ""
                                                  ? " / " + txRow.modelData.subcategory : "")
                                            : "Pick a category for this row first"
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

                // Footer actions
                RowLayout {
                    visible: !controller.lastParseFailed
                    Layout.fillWidth: true
                    spacing: 10

                    PillButton { text: "View log"; variant: "neutral"; onClicked: logPopup.open() }
                    PillButton {
                        text: "Import another file"
                        variant: "neutral"
                        onClicked: { controller.reset(); page.step = 0 }
                    }
                    Item { Layout.fillWidth: true }
                    PillButton {
                        text: page.saveBusy ? "Saving…" : "Save"
                        variant: "primary"
                        enabled: !page.saveBusy && page.selectedAccount !== null
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
                    title: "Import report — " + (reportStep.report.accountLabel ? reportStep.report.accountLabel : "")

                    ColumnLayout {
                        id: reportSummaryCol
                        anchors.fill: parent
                        spacing: 14

                        GridLayout {
                            columns: 4
                            columnSpacing: 24
                            rowSpacing: 6
                            Layout.fillWidth: true

                            Text { text: "Transactions saved"; font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "Total income";       font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "Total expenses";     font.pixelSize: 13; color: "#6c7086" }
                            Text { text: "Net change";         font.pixelSize: 13; color: "#6c7086" }

                            Text { text: reportStep.report.transactionCount !== undefined ? reportStep.report.transactionCount : 0; font.pixelSize: 18; font.bold: true; color: "#cdd6f4" }
                            Text { text: reportStep.report.totalIncomeText ? reportStep.report.totalIncomeText : "—"; font.pixelSize: 18; font.bold: true; color: "#a6e3a1" }
                            Text { text: reportStep.report.totalExpenseText ? reportStep.report.totalExpenseText : "—"; font.pixelSize: 18; font.bold: true; color: "#f38ba8" }
                            Text { text: reportStep.report.netChangeText ? reportStep.report.netChangeText : "—"; font.pixelSize: 18; font.bold: true; color: "#cdd6f4" }
                        }

                        // Rows that this account already had (a re-import, or a
                        // statement that overlaps an earlier one): save() skips
                        // them, and this line tells the user why the count is lower.
                        Text {
                            visible: (reportStep.report.alreadySavedCount || 0) > 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: (reportStep.report.alreadySavedCount || 0)
                                + " transaction(s) of this statement were already saved, so they were not added again."
                            font.pixelSize: 13
                            color: "#f9e2af"
                        }
                    }
                }

                // ── Reconciliation results ──────────────────────────────────────
                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: reconCol.implicitHeight + 50
                    title: "Reconciliation"

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
                                text: !reportStep.reconciliation.hasStartReference ? "Start balance — no prior record"
                                    : (reportStep.reconciliation.startOk ? "Start balance OK" : "Start balance mismatch")
                            }
                            Text {
                                visible: reportStep.reconciliation.hasStartReference === true
                                text: "declared " + (reportStep.reconciliation.startDeclaredText || "—")
                                    + " vs. " + (reportStep.reconciliation.startExpectedText || "—")
                                    + " (" + (reportStep.reconciliation.startSourceLabel || "") + " on "
                                    + (reportStep.reconciliation.startSourceDateText || "—") + ") — diff "
                                    + (reportStep.reconciliation.startDiffText || "—")
                                font.pixelSize: 13
                                color: "#6c7086"
                            }
                        }

                        RowLayout {
                            spacing: 10
                            StatusBadge {
                                pillHeight: 22
                                variant: reportStep.reconciliation.endOk ? "success" : "danger"
                                text: reportStep.reconciliation.endOk ? "End balance OK" : "End balance mismatch"
                            }
                            Text {
                                text: "computed " + (reportStep.reconciliation.endComputedText || "—")
                                    + " vs. declared " + (reportStep.reconciliation.endDeclaredText || "—")
                                    + " — diff " + (reportStep.reconciliation.endDiffText || "—")
                                font.pixelSize: 13
                                color: "#6c7086"
                            }
                        }
                    }
                }

                // ── Category / subcategory breakdown — pie chart + legend ───────
                ChartCard {
                    Layout.fillWidth: true
                    implicitHeight: breakdownCol.implicitHeight + 50
                    title: "By category — click a slice"

                    ColumnLayout {
                        id: breakdownCol
                        anchors.fill: parent
                        spacing: 10

                        EmptyState {
                            visible: reportStep.byCategory.length === 0
                            message: "No transactions were saved."
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
                                pieSize: 0.75
                                holeSize: 0.42

                                function rebuild() {
                                    clear()
                                    pieSliceInfo.text = "Click a slice for details"
                                    var cats = reportStep.byCategory
                                    for (var i = 0; i < cats.length; i++) {
                                        var magnitude = Math.abs(cats[i].totalRaw)
                                        if (magnitude <= 0)
                                            continue
                                        var slice = append(cats[i].category, magnitude)
                                        slice.color = CategoryController.categoryColor(cats[i].category) || "#6c7086"
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
                            text: "Click a slice for details"
                            font.pixelSize: 14
                            color: "#6c7086"
                        }

                        Rectangle {
                            visible: reportStep.byCategory.length > 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: "#313244"
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
                                        border.color: CategoryController.categoryColor(catRow.modelData.category) || "#6c7086"
                                        Text {
                                            id: catBadge
                                            anchors.centerIn: parent
                                            text: catRow.modelData.category
                                            font.pixelSize: 13
                                            color: CategoryController.categoryColor(catRow.modelData.category) || "#6c7086"
                                        }
                                    }
                                    Text {
                                        text: catRow.modelData.totalText
                                        font.pixelSize: 15
                                        font.bold: true
                                        color: catRow.modelData.totalRaw >= 0 ? "#a6e3a1" : "#f38ba8"
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
                                            color: "#181825"
                                            border.color: CategoryController.subcategoryColor(catRow.modelData.category, subChip.modelData.subcategory) || "#313244"

                                            RowLayout {
                                                id: subChipRow
                                                anchors.centerIn: parent
                                                spacing: 6
                                                Text {
                                                    text: subChip.modelData.subcategory + " · " + subChip.modelData.totalText
                                                    font.pixelSize: 11
                                                    color: "#a6adc8"
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
                        text: "Import another file"
                        variant: "primary"
                        onClicked: { controller.reset(); page.step = 0 }
                    }
                    Item { Layout.fillWidth: true }
                    PillButton { text: "Done"; variant: "neutral"; onClicked: page.closeRequested() }
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
            color: "#181825"
            border.color: "#45475a"
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Import log"; font.pixelSize: 18; font.bold: true; color: "#cdd6f4" }
                Item { Layout.fillWidth: true }
                Text {
                    text: controller.errorCount + " error(s) · " + controller.warningCount + " warning(s)"
                    font.pixelSize: 14
                    color: "#6c7086"
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
                        message: "No log entries."
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
                                    text: logRow.modelData.level
                                }
                                Text {
                                    text: "L" + logRow.modelData.line
                                    font.pixelSize: 13
                                    color: "#6c7086"
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: logRow.modelData.message
                                    font.pixelSize: 15
                                    color: "#cdd6f4"
                                    wrapMode: Text.WordWrap
                                }
                            }
                            Text {
                                visible: logRow.modelData.rawLine.length > 0
                                Layout.fillWidth: true
                                Layout.leftMargin: 70
                                text: logRow.modelData.rawLine
                                font.pixelSize: 13
                                font.family: "monospace"
                                color: "#45475a"
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                PillButton { text: "Close"; variant: "neutral"; onClicked: logPopup.close() }
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
            color: "#181825"
            border.color: "#45475a"
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text { text: "Full description"; font.pixelSize: 18; font.bold: true; color: "#cdd6f4" }

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
                    color: "#cdd6f4"
                    selectionColor: "#89b4fa"
                    selectedTextColor: "#11111b"
                }
            }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                PillButton { text: "Close"; variant: "neutral"; onClicked: descriptionPopup.close() }
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
            color: "#181825"
            border.color: "#f38ba8"
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text { text: "Balance doesn't add up"; font.pixelSize: 18; font.bold: true; color: "#cdd6f4" }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "The statement declares an ending balance of "
                    + (reconciliationDialog.check.endDeclaredText || "—")
                    + ", but the declared starting balance plus every currently-kept transaction only "
                    + "adds up to " + (reconciliationDialog.check.endComputedText || "—")
                    + " — a difference of " + (reconciliationDialog.check.endDiffText || "—") + "."
                font.pixelSize: 14
                color: "#cdd6f4"
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Either a kept transaction was unchecked by mistake, or a real amount is missing. "
                    + "Review the KEEP column, or add a matching adjustment transaction (category "
                    + "\"Reconciliation Adjustment\") to close the gap."
                font.pixelSize: 13
                color: "#6c7086"
            }

            RowLayout {
                Layout.topMargin: 6
                Layout.fillWidth: true
                spacing: 10
                PillButton {
                    text: "Review kept transactions"
                    variant: "neutral"
                    onClicked: reconciliationDialog.close()
                }
                Item { Layout.fillWidth: true }
                PillButton {
                    text: "Add adjustment transaction"
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
}