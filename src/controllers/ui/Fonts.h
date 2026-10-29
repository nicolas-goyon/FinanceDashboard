#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

// Fonts — the fonts that QML cannot name itself (#77).
//
// QML `font.family: "monospace"` names a family that Windows does not have,
// and the QML font type has no fallback list and no style hint. So Windows
// can pick a proportional font for the hashes and the logs. `monospace` gives
// the fixed-pitch font of the system (QFontDatabase::FixedFont), for example
// Courier New on Windows and the fontconfig monospace font on Linux. Some
// platforms give a generic name ("monospace"), so QFontInfo reads the family
// that the font database really uses, with the TypeWriter style hint.
//
// QML: `font.family: Fonts.monospace`.
class Fonts : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString monospace READ monospace CONSTANT)

public:
    using QObject::QObject;

    [[nodiscard]] static QString monospace()
    {
        QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setStyleHint(QFont::TypeWriter);
        return QFontInfo(font).family();
    }
};
