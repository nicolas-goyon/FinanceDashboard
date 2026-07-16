import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs
import FinanceDashboard

// ── Projects — PROJECT.md page 5 ──────────────────────────────────────────────
// LIST view: all projects as cards + "New project" form (Popup).
// DETAIL view: KPIs · projection vs target chart · loan simulator.
// Projects are [MOCK]; loan scenarios use the real annuity formula (C++).

Item {
    id: page

    property bool showDetail: false

    // Labeled TextField row used by the New-project form
    component FormField: RowLayout {
        property alias label: lbl.text
        property alias field: input
        property alias placeholder: input.placeholderText
        spacing: 10
        Text {
            id: lbl
            Layout.preferredWidth: 150
            font.pixelSize: 15
            color: "#6c7086"
        }
        TextField {
            id: input
            Layout.fillWidth: true
            color: "#cdd6f4"
            font.pixelSize: 15
            placeholderTextColor: "#45475a"
            background: Rectangle {
                color: "#11111b"
                radius: 4
                border.color: input.activeFocus ? "#89b4fa" : "#313244"
            }
        }
    }

    ProjectsController { id: controller }

    function rebuildChart() {
        projectionSeries.clear()
        targetSeries.clear()
        var t = controller.projectionTimestamps()
        var v = controller.projectionValues()
        for (var i = 0; i < t.length; i++)
            projectionSeries.append(t[i], v[i])
        if (t.length > 0) {
            targetSeries.append(t[0], controller.targetAmount)
            targetSeries.append(t[t.length - 1], controller.targetAmount)
        }
    }

    Component.onCompleted: rebuildChart()
    Connections {
        target: controller
        function onProjectChanged() { rebuildChart() }
    }

    // ══════════════════════════ LIST VIEW ════════════════════════════════════
    ScrollView {
        visible: !page.showDetail
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            RowLayout {
                spacing: 12
                Text {
                    text: "Projects"
                    font.pixelSize: 23
                    font.bold: true
                    color: "#cdd6f4"
                }
                Text { text: "[MOCK]"; font.pixelSize: 14; color: "#f38ba8" }
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 130; height: 32; radius: 4
                    color: newHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text {
                        anchors.centerIn: parent
                        text: "＋  New project"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#cdd6f4"
                    }
                    HoverHandler { id: newHover }
                    TapHandler { onTapped: newProjectDialog.open() }
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 14

                Repeater {
                    model: controller.projectCards
                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        width: 320
                        height: cardCol.implicitHeight + 28
                        radius: 8
                        color: cardHover.hovered ? "#232336" : "#1e1e2e"
                        border.color: cardHover.hovered ? "#89b4fa" : "#313244"

                        HoverHandler { id: cardHover }
                        TapHandler {
                            onTapped: {
                                controller.currentProject = index
                                page.showDetail = true
                            }
                        }

                        ColumnLayout {
                            id: cardCol
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    font.pixelSize: 18
                                    font.bold: true
                                    color: "#cdd6f4"
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    width: onTrackText.implicitWidth + 14
                                    height: 20
                                    radius: 10
                                    color: modelData.onTrack ? "#1a3328" : "#3d1a28"
                                    Text {
                                        id: onTrackText
                                        anchors.centerIn: parent
                                        text: modelData.onTrack ? "on track" : "gap " + modelData.gapText
                                        font.pixelSize: 13
                                        color: modelData.onTrack ? "#a6e3a1" : "#f38ba8"
                                    }
                                }
                            }

                            // Progress bar (earmarked / target)
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: "#11111b"
                                    Rectangle {
                                        width: parent.width * modelData.progressPct
                                        height: parent.height
                                        radius: 4
                                        color: "#89b4fa"
                                    }
                                }
                                Text {
                                    text: Math.round(modelData.progressPct * 100) + " %"
                                    font.pixelSize: 14
                                    color: "#6c7086"
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: 3
                                columnSpacing: 16

                                Text { text: "Target";     font.pixelSize: 13; color: "#6c7086" }
                                Text { text: modelData.targetText;     font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                                Text { text: "Saved";      font.pixelSize: 13; color: "#6c7086" }
                                Text { text: modelData.earmarkedText + "   (" + modelData.monthlyText + ")"; font.pixelSize: 15; color: "#cdd6f4" }
                                Text { text: "Target date"; font.pixelSize: 13; color: "#6c7086" }
                                Text { text: modelData.targetDateText; font.pixelSize: 15; color: "#cdd6f4" }
                                Text { text: "Completion"; font.pixelSize: 13; color: "#6c7086" }
                                Text { text: modelData.completionText; font.pixelSize: 15; color: modelData.onTrack ? "#a6e3a1" : "#f9e2af" }
                            }
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }

    // ══════════════════════════ DETAIL VIEW ══════════════════════════════════
    ScrollView {
        visible: page.showDetail
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            RowLayout {
                spacing: 12
                Rectangle {
                    width: 110; height: 30; radius: 4
                    color: backHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text {
                        anchors.centerIn: parent
                        text: "←  All projects"
                        font.pixelSize: 15
                        color: "#cdd6f4"
                    }
                    HoverHandler { id: backHover }
                    TapHandler { onTapped: page.showDetail = false }
                }
                Text {
                    text: controller.projectNames[controller.currentProject] ?? ""
                    font.pixelSize: 23
                    font.bold: true
                    color: "#cdd6f4"
                }
                Item { Layout.fillWidth: true }
            }

            // ── KPI cards ─────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Repeater {
                    model: controller.kpis
                    delegate: KpiCard {
                        required property var modelData
                        Layout.fillWidth: true
                        label: modelData.label
                        value: modelData.value
                        sub: modelData.sub
                        valueColor: modelData.color
                    }
                }
            }

            // ── Projection vs target ──────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: 340
                title: "Projected savings vs target (€)"

                GraphsView {
                    anchors.fill: parent
                    theme: GraphsTheme {
                        colorScheme: GraphsTheme.ColorScheme.Dark
                        plotAreaBackgroundVisible: true
                    }

                    axisX: DateTimeAxis {
                        min: new Date(controller.minTimeMs)
                        max: new Date(controller.maxTimeMs)
                        labelFormat: "MMM yy"
                    }
                    axisY: ValueAxis {
                        min: 0
                        max: controller.chartMax
                        labelFormat: "%.0f"
                    }

                    LineSeries {
                        id: projectionSeries
                        name: "Projected savings"
                        color: "#89b4fa"
                        width: 2.5
                        pointDelegate: Component {
                            HoverPoint { seriesName: "Projected"; pointColor: "#89b4fa" }
                        }
                    }
                    LineSeries {
                        id: targetSeries
                        name: "Target"
                        color: "#f38ba8"
                        width: 2
                    }
                }
            }

            // ── Loan simulator ────────────────────────────────────────────────
            ChartCard {
                Layout.fillWidth: true
                implicitHeight: loanCol.implicitHeight + 50
                title: "Loan simulator — funding the gap"

                ColumnLayout {
                    id: loanCol
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        spacing: 10
                        Text { text: "Annual rate:"; font.pixelSize: 15; color: "#6c7086" }
                        TextField {
                            id: rateField
                            Layout.preferredWidth: 80
                            text: controller.loanRatePct.toFixed(2)
                            color: "#cdd6f4"
                            font.pixelSize: 15
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            background: Rectangle {
                                color: "#11111b"
                                radius: 4
                                border.color: rateField.activeFocus ? "#89b4fa" : "#313244"
                            }
                            onEditingFinished: {
                                var v = parseFloat(text)
                                if (!isNaN(v)) controller.loanRatePct = v
                            }
                        }
                        Text { text: "%   —   rates are entered manually (no market data)"; font.pixelSize: 14; color: "#6c7086" }
                        Item { Layout.fillWidth: true }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        color: "#181825"
                        radius: 4
                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            Text { Layout.preferredWidth: 180; text: "Scenario";        font.pixelSize: 14; font.bold: true; color: "#6c7086" }
                            Text { Layout.fillWidth: true;     text: "Monthly payment"; font.pixelSize: 14; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 140; text: "Total interest";  font.pixelSize: 14; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                            Text { Layout.preferredWidth: 140; text: "Total cost";      font.pixelSize: 14; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                        }
                    }
                    Repeater {
                        model: controller.loanScenarios
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            color: index % 2 === 0 ? "transparent" : "#181825"
                            radius: 4
                            RowLayout {
                                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                Text { Layout.preferredWidth: 180; text: modelData.label;         font.pixelSize: 15; color: "#cdd6f4" }
                                Text { Layout.fillWidth: true;     text: modelData.monthly;       font.pixelSize: 15; font.bold: true; color: "#89b4fa"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 140; text: modelData.totalInterest; font.pixelSize: 15; color: "#f38ba8"; horizontalAlignment: Text.AlignRight }
                                Text { Layout.preferredWidth: 140; text: modelData.totalCost;     font.pixelSize: 15; color: "#cdd6f4"; horizontalAlignment: Text.AlignRight }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: saveVsBorrow.implicitHeight + 20
                        color: "#181825"
                        radius: 6
                        border.color: "#313244"
                        Text {
                            id: saveVsBorrow
                            anchors { fill: parent; margins: 10 }
                            text: "⚖  " + controller.saveVsBorrowText
                            wrapMode: Text.WordWrap
                            font.pixelSize: 15
                            color: "#cdd6f4"
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }

    // ══════════════════════════ NEW PROJECT DIALOG ═══════════════════════════
    Popup {
        id: newProjectDialog
        anchors.centerIn: parent
        width: 420
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
                text: "New project"
                font.pixelSize: 19
                font.bold: true
                color: "#cdd6f4"
            }

            FormField { id: fName;      label: "Name";                 placeholder: "e.g. House deposit" }
            FormField { id: fTarget;    label: "Target amount (€)";    placeholder: "40000" }
            FormField { id: fMonths;    label: "Months until target";  placeholder: "36" }
            FormField { id: fEarmarked; label: "Already saved (€)";    placeholder: "0" }
            FormField { id: fMonthly;   label: "Monthly contribution (€)"; placeholder: "300" }

            RowLayout {
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 80; height: 30; radius: 4
                    color: cancelHover.hovered ? "#313244" : "#1e1e2e"
                    border.color: "#313244"
                    Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 15; color: "#cdd6f4" }
                    HoverHandler { id: cancelHover }
                    TapHandler { onTapped: newProjectDialog.close() }
                }
                Rectangle {
                    width: 90; height: 30; radius: 4
                    color: createHover.hovered ? "#264d80" : "#1e3a5f"
                    border.color: "#89b4fa"
                    Text { anchors.centerIn: parent; text: "Create"; font.pixelSize: 15; font.bold: true; color: "#cdd6f4" }
                    HoverHandler { id: createHover }
                    TapHandler {
                        onTapped: {
                            var target = parseFloat(fTarget.field.text)
                            var months = parseInt(fMonths.field.text)
                            var saved = parseFloat(fEarmarked.field.text)
                            var monthly = parseFloat(fMonthly.field.text)
                            if (fName.field.text.trim().length === 0
                                    || isNaN(target) || isNaN(months))
                                return
                            controller.addProject(fName.field.text,
                                                  target, months,
                                                  isNaN(saved) ? 0 : saved,
                                                  isNaN(monthly) ? 0 : monthly)
                            fName.field.text = ""
                            fTarget.field.text = ""
                            fMonths.field.text = ""
                            fEarmarked.field.text = ""
                            fMonthly.field.text = ""
                            newProjectDialog.close()
                            page.showDetail = true   // controller selected the new one
                        }
                    }
                }
            }
        }
    }
}
