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

            // ── Header ────────────────────────────────────────────────────────
            RowLayout {
                spacing: 12
                Rectangle {
                    width: 26; height: 26; radius: 4
                    color: backHover.hovered ? "#181825" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "←"
                        font.pixelSize: 19
                        color: "#89b4fa"
                    }
                    HoverHandler { id: backHover }
                    TapHandler { onTapped: page.closeRequested() }
                }
                Text {
                    text: "Import Transactions"
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
                    color: controller.lastParseFailed ? "#3d2028" : "#1a3328"
                    border.color: controller.lastParseFailed ? "#5a2d36" : "#2d5a45"
                    Text {
                        id: statusLabel
                        anchors.centerIn: parent
                        text: controller.statusText
                        font.pixelSize: 14
                        color: controller.lastParseFailed ? "#f38ba8" : "#a6e3a1"
                    }
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

                    Rectangle {
                        Layout.alignment: Qt.AlignRight
                        width: 170; height: 32; radius: 4
                        color: importHover.hovered ? "#264d80" : "#1e3a5f"
                        border.color: "#89b4fa"
                        Text {
                            anchors.centerIn: parent
                            text: "Choose file & Import"
                            font.pixelSize: 15
                            font.bold: true
                            color: "#cdd6f4"
                        }
                        HoverHandler { id: importHover }
                        TapHandler { onTapped: fileDialog.open() }
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
                            Rectangle {
                                width: 90; height: 28; radius: 4
                                color: "#1e1e2e"; border.color: "#313244"
                                Text { anchors.centerIn: parent; text: "View log"; font.pixelSize: 14; color: "#cdd6f4" }
                                TapHandler { onTapped: logPopup.open() }
                            }
                            Rectangle {
                                width: 90; height: 28; radius: 4
                                color: "#1e1e2e"; border.color: "#313244"
                                Text { anchors.centerIn: parent; text: "Try again"; font.pixelSize: 14; color: "#cdd6f4" }
                                TapHandler { onTapped: { controller.reset(); page.step = 0 } }
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
                            Rectangle {
                                width: logButtonRow.implicitWidth + 16
                                height: 22
                                radius: 11
                                color: controller.errorCount > 0 ? "#3d2028" : (controller.warningCount > 0 ? "#3d3320" : "#181825")
                                border.color: "#313244"
                                RowLayout {
                                    id: logButtonRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text {
                                        text: (controller.errorCount + controller.warningCount) === 0
                                              ? "No issues" : (controller.errorCount + " error(s), " + controller.warningCount + " warning(s)")
                                        font.pixelSize: 13
                                        color: controller.errorCount > 0 ? "#f38ba8" : (controller.warningCount > 0 ? "#f9e2af" : "#a6e3a1")
                                    }
                                }
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

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#313244" }

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

                            Rectangle {
                                visible: page.selectedAccount !== null && !page.selectedAccount.linked
                                width: notLinkedRow.implicitWidth + 16
                                height: 24
                                radius: 12
                                color: "#3d3320"
                                border.color: "#5a4d2d"
                                RowLayout {
                                    id: notLinkedRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text {
                                        text: "⚠ Not linked — no hash set up for this account"
                                        font.pixelSize: 13
                                        color: "#f9e2af"
                                    }
                                }
                            }
                            Rectangle {
                                visible: page.selectedAccount !== null && page.selectedAccount.linked
                                width: linkedRow.implicitWidth + 16
                                height: 24
                                radius: 12
                                color: "#1a3328"
                                border.color: "#2d5a45"
                                RowLayout {
                                    id: linkedRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text {
                                        text: "✓ Linked (" + page.selectedAccount.hashCount + " hash"
                                            + (page.selectedAccount.hashCount === 1 ? "" : "es") + ")"
                                        font.pixelSize: 13
                                        color: "#a6e3a1"
                                    }
                                }
                            }

                            // Offer to link this statement's fingerprint to
                            // the selected account right here, when it has
                            // one to give and the account doesn't have it
                            // yet — saves a trip to AccountsManagePage.qml.
                            Rectangle {
                                visible: controller.statementInfo.accountHashRaw.length > 0
                                    && page.selectedAccount !== null
                                    && !AccountController.accountHasHash(page.selectedAccountIndex, controller.statementInfo.accountHashRaw)
                                width: 150; height: 24; radius: 4
                                color: linkHover.hovered ? "#264d80" : "#1e3a5f"
                                border.color: "#89b4fa"
                                Text { anchors.centerIn: parent; text: "Link this account"; font.pixelSize: 13; color: "#cdd6f4" }
                                HoverHandler { id: linkHover }
                                TapHandler {
                                    onTapped: AccountController.linkAccountHash(
                                        page.selectedAccountIndex, controller.statementInfo.accountHashRaw)
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
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                color: index % 2 === 0 ? "transparent" : "#181825"
                                opacity: modelData.included ? 1.0 : 0.4

                                RowLayout {
                                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                    spacing: 8

                                    Text { Layout.preferredWidth: 80; text: modelData.operationDate; font.pixelSize: 14; color: "#6c7086" }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.description
                                        font.pixelSize: 15
                                        color: "#cdd6f4"
                                        elide: Text.ElideRight
                                    }
                                    Text { Layout.preferredWidth: 90;  text: modelData.debitText;  font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 90;  text: modelData.creditText; font.pixelSize: 15; color: "#a6e3a1"; horizontalAlignment: Text.AlignRight }
                                    Text { Layout.preferredWidth: 100; text: modelData.balanceText; font.pixelSize: 14; color: "#6c7086"; horizontalAlignment: Text.AlignRight }

                                    ColorComboBox {
                                        id: categoryBox
                                        Layout.preferredWidth: 115
                                        Layout.preferredHeight: 36
                                        items: CategoryController.categories
                                        currentName: modelData.category
                                        onNameActivated: (name) => controller.setRowCategory(modelData.index, name)
                                    }

                                    ColorComboBox {
                                        Layout.preferredWidth: 115
                                        Layout.preferredHeight: 36
                                        readonly property var subItems: {
                                            var cat = CategoryController.categories.find(function(c) { return c.name === modelData.category })
                                            return cat ? cat.subcategories : []
                                        }
                                        items: subItems
                                        enabled: subItems.length > 0
                                        currentName: modelData.subcategory
                                        onNameActivated: (name) => controller.setRowSubcategory(modelData.index, name)
                                    }

                                    CheckBox {
                                        Layout.preferredWidth: 50
                                        checked: modelData.included
                                        onCheckedChanged: controller.setRowIncluded(modelData.index, checked)
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

                    Rectangle {
                        width: 130; height: 30; radius: 4
                        color: "#1e1e2e"; border.color: "#313244"
                        Text { anchors.centerIn: parent; text: "View log"; font.pixelSize: 15; color: "#cdd6f4" }
                        TapHandler { onTapped: logPopup.open() }
                    }
                    Rectangle {
                        width: 150; height: 30; radius: 4
                        color: "#1e1e2e"; border.color: "#313244"
                        Text { anchors.centerIn: parent; text: "Import another file"; font.pixelSize: 15; color: "#cdd6f4" }
                        TapHandler { onTapped: { controller.reset(); page.step = 0 } }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        width: 100; height: 30; radius: 4
                        color: saveHover.hovered ? "#264d80" : "#1e3a5f"
                        border.color: "#89b4fa"
                        Text { anchors.centerIn: parent; text: "Save"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                        HoverHandler { id: saveHover }
                        TapHandler {
                            onTapped: {
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
            }

            Item { height: 8 }
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

                    Text {
                        visible: controller.logEntries.length === 0
                        text: "No log entries."
                        font.pixelSize: 15
                        color: "#6c7086"
                    }

                    Repeater {
                        model: controller.logEntries
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                spacing: 8
                                Rectangle {
                                    width: 62; height: 18; radius: 9
                                    color: modelData.level === "Error" ? "#3d2028"
                                         : modelData.level === "Warning" ? "#3d3320" : "#1e1e2e"
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.level
                                        font.pixelSize: 12
                                        color: modelData.level === "Error" ? "#f38ba8"
                                             : modelData.level === "Warning" ? "#f9e2af" : "#a6e3a1"
                                    }
                                }
                                Text {
                                    text: "L" + modelData.line
                                    font.pixelSize: 13
                                    color: "#6c7086"
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.message
                                    font.pixelSize: 15
                                    color: "#cdd6f4"
                                    wrapMode: Text.WordWrap
                                }
                            }
                            Text {
                                visible: modelData.rawLine.length > 0
                                Layout.fillWidth: true
                                Layout.leftMargin: 70
                                text: modelData.rawLine
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
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: logCloseHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Close"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: logCloseHover }
                    TapHandler { onTapped: logPopup.close() }
                }
            }
        }
    }
}