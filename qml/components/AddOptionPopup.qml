pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// ── AddOptionPopup ───────────────────────────────────────────────────────────
// The popup of a ComboBox, with a last row "＋ New…" that adds an option to the
// list (#237). AddOptionComboBox and ColorComboBox use it. With an empty
// `addText`, there is no last row: the popup is the same as the Basic one.
//
// The popup is a copy of the popup of the Basic style (qtdeclarative 6.10,
// src/quickcontrols/basic/ComboBox.qml): the same size, margins, ListView and
// border. The only change is the ListView `footer`. The last row is a footer,
// not an item of the model, so the model, `count` and `currentIndex` of the
// ComboBox do not change, and a click on it does not emit `activated`.
// A click closes the popup, then emits `addClicked()`: the caller opens its
// dialog, adds the option and selects it.
Popup {
    id: root

    // The ComboBox of this popup.
    required property ComboBox comboBox
    // Text of the last row, for example qsTr("＋ New category…"). Empty: no
    // last row.
    property string addText: ""

    signal addClicked()

    y: root.comboBox.height
    width: root.comboBox.width
    height: Math.min(contentItem.implicitHeight, root.comboBox.Window.height - topMargin - bottomMargin)
    topMargin: 6
    bottomMargin: 6
    padding: 0
    palette: root.comboBox.palette

    contentItem: ListView {
        id: list
        clip: true
        implicitHeight: contentHeight
        model: root.comboBox.delegateModel
        currentIndex: root.comboBox.highlightedIndex
        highlightMoveDuration: 0
        footer: root.addText.length > 0 ? addRowComponent : null

        Rectangle {
            z: 10
            width: parent.width
            height: parent.height
            color: "transparent"
            border.color: root.comboBox.palette.mid
        }

        ScrollIndicator.vertical: ScrollIndicator { }
    }

    background: Rectangle {
        color: root.comboBox.palette.window
    }

    Component {
        id: addRowComponent

        ItemDelegate {
            id: addRow
            width: list.width
            text: root.addText
            // The font of the ComboBox, as the rows of the Basic style.
            font: root.comboBox.font

            contentItem: Text {
                text: addRow.text
                color: Theme.green
                font: addRow.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: addRow.hovered ? Theme.surfaceHover : Theme.base
                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.surface0
                }
            }

            onClicked: {
                root.close()
                root.addClicked()
            }
        }
    }
}
