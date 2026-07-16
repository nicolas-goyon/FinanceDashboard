import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Portfolio tab ─────────────────────────────────────────────────────────────
// Holdings table. All P&L math and formatting is done in PortfolioController
// (still debug samples — see ARCHITECTURE.MD).

Item {
    PortfolioController { id: controller }

    // Column widths (Name stretches)
    readonly property var colWidths: [80, -1, 90, 110, 110, 130, 120, 90]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        Text {
            text: "Holdings"
            font { pixelSize: 20; bold: true }
            color: "#cdd6f4"
        }
        Text {
            text: "[DEBUG DATA — not live]"
            font.pixelSize: 11
            color: "#f38ba8"
        }

        // ── Header row ────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 30
            color: "#181825"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Text { Layout.preferredWidth: colWidths[0]; text: "Ticker";       font.pixelSize: 11; font.bold: true; color: "#6c7086" }
                Text { Layout.fillWidth: true;              text: "Name";         font.pixelSize: 11; font.bold: true; color: "#6c7086" }
                Text { Layout.preferredWidth: colWidths[2]; text: "Shares";       font.pixelSize: 11; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                Text { Layout.preferredWidth: colWidths[3]; text: "Avg Cost";     font.pixelSize: 11; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                Text { Layout.preferredWidth: colWidths[4]; text: "Price";        font.pixelSize: 11; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                Text { Layout.preferredWidth: colWidths[5]; text: "Market Value"; font.pixelSize: 11; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                Text { Layout.preferredWidth: colWidths[6]; text: "P&L";          font.pixelSize: 11; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
                Text { Layout.preferredWidth: colWidths[7]; text: "P&L %";        font.pixelSize: 11; font.bold: true; color: "#6c7086"; horizontalAlignment: Text.AlignRight }
            }
        }

        // ── Rows ──────────────────────────────────────────────────────────────
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: controller.rows

            delegate: Rectangle {
                required property var modelData
                required property int index

                width: ListView.view.width
                height: 34
                color: index % 2 === 0 ? "#11111b" : "#1e1e2e"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text { Layout.preferredWidth: colWidths[0]; text: modelData.ticker;       font.pixelSize: 13; color: "#cdd6f4" }
                    Text { Layout.fillWidth: true;              text: modelData.name;         font.pixelSize: 13; color: "#cdd6f4"; elide: Text.ElideRight }
                    Text { Layout.preferredWidth: colWidths[2]; text: modelData.shares;       font.pixelSize: 13; color: "#cdd6f4"; horizontalAlignment: Text.AlignRight }
                    Text { Layout.preferredWidth: colWidths[3]; text: modelData.avgCost;      font.pixelSize: 13; color: "#cdd6f4"; horizontalAlignment: Text.AlignRight }
                    Text { Layout.preferredWidth: colWidths[4]; text: modelData.price;        font.pixelSize: 13; color: "#cdd6f4"; horizontalAlignment: Text.AlignRight }
                    Text { Layout.preferredWidth: colWidths[5]; text: modelData.marketValue;  font.pixelSize: 13; color: "#cdd6f4"; horizontalAlignment: Text.AlignRight }
                    Text { Layout.preferredWidth: colWidths[6]; text: modelData.pnl;          font.pixelSize: 13; color: modelData.positive ? "#a6e3a1" : "#f38ba8"; horizontalAlignment: Text.AlignRight }
                    Text { Layout.preferredWidth: colWidths[7]; text: modelData.pnlPct;       font.pixelSize: 13; color: modelData.positive ? "#a6e3a1" : "#f38ba8"; horizontalAlignment: Text.AlignRight }
                }

                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 1
                    color: "#313244"
                }
            }
        }

        // ── Footer ────────────────────────────────────────────────────────────
        Text {
            text: controller.footerText
            font.pixelSize: 12
            color: "#6c7086"
        }
    }
}
