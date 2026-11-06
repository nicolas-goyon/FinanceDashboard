pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FinanceDashboard

// ── Settings ──────────────────────────────────────────────────────────────────
// The preferences of this computer. Language: the Language singleton
// (src/controllers/ui/Language.h) keeps the choice in QSettings and
// translates the app at once (QQmlEngine::retranslate()).
Item {
    id: page

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            PageHeader {
                title: qsTr("Settings")
            }

            ChartCard {
                Layout.fillWidth: true
                implicitHeight: languageCol.implicitHeight + 50
                title: qsTr("Language")

                ColumnLayout {
                    id: languageCol
                    anchors.fill: parent
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("The language of the app. \"System language\" follows the language of this "
                                       + "computer (French or English).")
                            font.pixelSize: 14
                            color: Theme.overlay0
                        }

                        ComboBox {
                            id: languageBox
                            Layout.preferredWidth: 220
                            model: Language.choices
                            textRole: "name"
                            valueRole: "code"
                            // Bound to the singleton: it stays right after a change.
                            currentIndex: indexOfValue(Language.language)
                            onActivated: Language.language = currentValue
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("The names of your categories are data: they do not change with the language. "
                                   + "A new database, or \"Reset categories to the defaults\", gets the default "
                                   + "categories in the language of the app.")
                        font.pixelSize: 13
                        color: Theme.overlay0
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
