import QtQuick
import FinanceDashboard

// ── DateAxisLabel ────────────────────────────────────────────────────────────
// The label delegate of a DateTimeAxis, with the month names of the language
// of the app. Qt Graphs formats a DateTimeAxis label with labelFormat, in
// English whatever the language. So the axis gives the date as numbers
// (labelFormat: "yyyy-MM-dd"), and this delegate shows it with
// `displayFormat` in the default locale (Language sets it).
//
//   axisX: DateTimeAxis {
//       labelFormat: "yyyy-MM-dd"
//       labelDelegate: Component { DateAxisLabel { displayFormat: "MMM yy" } }
//   }
//
// AbstractAxis.labelDelegate: "This component should either be a Text
// component or contain property string text so that this property will be
// assigned the value of the label" (Qt Graphs docs).
Item {
    id: root

    // Set by the axis: the date in the labelFormat of the axis, "yyyy-MM-dd".
    property string text
    // How to show it ("MMM yy", "dd MMM"…): a QLocale date format.
    property string displayFormat: "MMM yy"

    implicitWidth: label.implicitWidth
    implicitHeight: label.implicitHeight

    Text {
        id: label
        anchors.centerIn: parent
        // Read again when the language changes (Language.effectiveLanguage).
        text: {
            const language = Language.effectiveLanguage // dependency of the binding
            const parts = root.text.split("-")
            if (parts.length !== 3 || language.length === 0)
                return root.text
            const date = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]))
            return Qt.locale().toString(date, root.displayFormat)
        }
        font.pixelSize: 12
        color: Theme.subtext0
    }
}
