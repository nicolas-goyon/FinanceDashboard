import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import FinanceDashboard

// ── Import Transactions — statement import ────────────────────────────────────
// Reached from the sidebar: Transactions Management → "Import Transactions"
// action (renamed from "New Monthly Snapshot" / "Data Entry" — see
// ARCHITECTURE.MD § Transactions Management).
// Step 1: pick a template + file → real parsing (SnapshotImportController,
//         backed by src/import/ — see ARCHITECTURE.MD § Statement Import).
// Step 2: review header info, match/pick the target account, edit rows and
//         categories, inspect the parse log, save for real (see
//         SnapshotImportController class comment for scope — persists to
//         the `transactions` table and auto-links the account hash).

Item {
    id: page

    signal closeRequested()

    property int step: 0   // 0 = pick template/file · 1 = review/edit

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
                                text: "✓ Linked (" + page.selectedAccount.hashCount + " hash"
                                    + (page.selectedAccount.hashCount === 1 ? "" : "es") + ")"
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

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            color: "#181825"
                            radius: 4
                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                spacing: 8
                                Text { Layout.preferredWidth: 80;  text: "OP. DATE";  font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.fillWidth: true;     text: "DESCRIPTION"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.preferredWidth: 90;  text: "DEBIT";     font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 90;  text: "CREDIT";    font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 100; text: "BALANCE";   font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 115; text: "CATEGORY";    font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.preferredWidth: 115; text: "SUBCATEGORY"; font.pixelSize: 13; font.bold: true; color: "#6c7086" }
                                Text { Layout.preferredWidth: 50;  text: "KEEP";      font.pixelSize: 13; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
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

                                    Text { Layout.preferredWidth: 80; text: txRow.modelData.operationDate; font.pixelSize: 14; color: "#6c7086" }
                                    Text {
                                        Layout.fillWidth: true
                                        text: txRow.modelData.description
                                        font.pixelSize: 15
                                        color: "#cdd6f4"
                                        elide: Text.ElideRight
                                    }
                                    Text { Layout.preferredWidth: 90;  text: txRow.modelData.debitText;  font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 90;  text: txRow.modelData.creditText; font.pixelSize: 15; color: "#a6e3a1"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 100; text: txRow.modelData.balanceText; font.pixelSize: 14; color: "#6c7086"; horizontalAlignment: Text.AlignRight }

                                    ColorComboBox {
                                        id: categoryBox
                                        Layout.preferredWidth: 115
                                        Layout.preferredHeight: 36
                                        items: CategoryController.categories
                                        currentName: txRow.modelData.category
                                        onNameActivated: (name) => controller.setRowCategory(txRow.modelData.index, name)
                                    }

                                    ColorComboBox {
                                        Layout.preferredWidth: 115
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

                                    CheckBox {
                                        Layout.preferredWidth: 50
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
                        text: "Save"
                        variant: "primary"
                        onClicked: {
                            var accountId = page.selectedAccount !== null ? page.selectedAccount.id : -1
                            var label = page.selectedAccount !== null
                                ? page.selectedAccount.name
                                : "(no account selected)"
                            controller.save(accountId, label)
                            // Pick up the transactions/hash write this
                            // just made — see AccountController.reload()
                            // doc comment (no C++-to-C++ singleton
                            // coupling between the two controllers).
                            AccountController.reload()
                        }
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
}