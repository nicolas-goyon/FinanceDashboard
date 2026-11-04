#include "import/StatementSanitizer.h"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace StatementSanitizer {
namespace {

const QString& mask()
{
    static const QString kMaskText = kMask.toString();
    return kMaskText;
}

// A reference line of the bank ("External ref.: …", "Internal ref.: …",
// "End-to-end ref.: …"): the same test as the template uses to find the end
// of the name.
bool isReferenceLine(const QString& line)
{
    static const QRegularExpression kRefLine(
        QStringLiteral("\\bref\\.?\\s*:"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return kRefLine.match(line).hasMatch();
}

// Replaces each match of `pattern` for which `accept` is true by
// `replace(match)`.
template <typename Accept, typename Replace>
void replaceIf(QString& text, const QRegularExpression& pattern, Accept accept, Replace replace)
{
    QString out;
    qsizetype last = 0;
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (!accept(m)) {
            continue;
        }
        out += text.mid(last, m.capturedStart() - last);
        out += replace(m);
        last = m.capturedEnd();
    }
    if (last == 0) {
        return;
    }
    out += text.mid(last);
    text = out;
}

qsizetype digitCount(QStringView text)
{
    qsizetype n = 0;
    for (const QChar c : text) {
        n += c.isDigit() ? 1 : 0;
    }
    return n;
}

void maskIdentifiers(QString& text)
{
    using RE = QRegularExpression;
    const auto ci = RE::CaseInsensitiveOption | RE::UseUnicodePropertiesOption;

    // A labelled id: the label stays, the value goes. An id is long and has
    // digits (6 characters or more, one digit or more); a BIC has 8 or 11
    // letters and digits. So "ref: rent" and a short note ("ref: APT12")
    // stay.
    static const RE kLabelled(
        QStringLiteral("\\b(IBAN|BIC|SWIFT|RUM|ICS|Mandate(?:\\s+(?:id|ref(?:erence)?))?|Mandat|"
                       "Creditor\\s+id|Cr\u00e9ancier|Creancier|R\u00e9f(?:\u00e9rence)?|Ref(?:erence)?|"
                       "N\u00b0\\s*client|No\\s*client)\\b(\\s*[.:#]?\\s*)([A-Z0-9][A-Z0-9/.-]{3,})"),
        ci);
    replaceIf(text, kLabelled,
              [](const QRegularExpressionMatch& m) {
                  const QString label = m.captured(1).toUpper();
                  const qsizetype size = m.capturedView(3).size();
                  if (label == QLatin1String("BIC") || label == QLatin1String("SWIFT")) {
                      return size == 8 || size == 11;
                  }
                  return size >= 6 && digitCount(m.capturedView(3)) > 0;
              },
              [](const QRegularExpressionMatch& m) { return m.captured(1) + m.captured(2) + mask(); });

    // A card: "Card payment 1234", "Carte 1234", "CB 1234", "CB*1234".
    static const RE kCard(
        QStringLiteral("\\b(card(?:\\s+payment)?|carte|cb)(?:\\s*n[\u00b0o])?\\s*[*X\u2022]*\\s*\\d{4}\\b"), ci);
    replaceIf(text, kCard, [](const QRegularExpressionMatch&) { return true; },
              [](const QRegularExpressionMatch& m) { return m.captured(1) + QLatin1Char(' ') + mask(); });

    // A masked card number: "XXXX1234", "**** 1234", "1234 XXXX XXXX 5678".
    static const RE kMaskedCard(QStringLiteral("(?:\\b\\d{4} ?)?(?:[*X\u2022]{4} ?){1,3}\\d{4}\\b"), ci);
    replaceIf(text, kMaskedCard, [](const QRegularExpressionMatch&) { return true; },
              [](const QRegularExpressionMatch&) { return mask(); });

    // An IBAN: 2 capital letters, 2 check digits, then groups of 4 letters
    // or digits (with or without a space), and a last group of 1 to 4. At
    // least 10 digits: an IBAN has them, a merchant name does not. After the
    // labelled ids, so that "RUM: FR12… ICS: …" keeps its labels.
    static const RE kIban(QStringLiteral("\\b[A-Z]{2}\\d{2}(?: ?[A-Z0-9]{4}){2,7}(?: ?[A-Z0-9]{1,4})?\\b"));
    replaceIf(text, kIban, [](const QRegularExpressionMatch& m) { return digitCount(m.capturedView()) >= 10; },
              [](const QRegularExpressionMatch&) { return mask(); });

    // 10 digits or more, also in groups: an account number, a client code.
    static const RE kLongNumber(QStringLiteral("\\b\\d(?: ?\\d){9,}\\b"));
    replaceIf(text, kLongNumber, [](const QRegularExpressionMatch&) { return true; },
              [](const QRegularExpressionMatch&) { return mask(); });
}

} // namespace

QString clean(const QString& text, const QStringList& secrets)
{
    QString out = text;
    for (const QString& secret : secrets) {
        const QString value = secret.simplified();
        if (value.size() < 3) {
            continue; // too short to be an identifier, and it would mask common words
        }
        out.replace(value, mask(), Qt::CaseInsensitive);
        // An IBAN or a number can be written with or without its spaces.
        const QString compact = QString(value).remove(QLatin1Char(' '));
        if (compact != value && compact.size() >= 3) {
            out.replace(compact, mask(), Qt::CaseInsensitive);
        }
    }
    maskIdentifiers(out);
    // Two masks side by side ("••• •••") read as one.
    static const QRegularExpression kDoubleMask(QStringLiteral("\u2022{3}(?:\\s*\u2022{3})+"));
    out.replace(kDoubleMask, mask());
    return out;
}

QString cleanDescription(const QString& rawDescription, const QStringList& secrets)
{
    QStringList kept;
    for (const QString& rawLine : rawDescription.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty() && !isReferenceLine(line)) {
            kept << line;
        }
    }
    // Every line is a reference line: keep the first one, without its value,
    // so that the description is not empty when the bank wrote something.
    if (kept.isEmpty()) {
        const QString first = rawDescription.section(QLatin1Char('\n'), 0, 0).trimmed();
        if (!first.isEmpty()) {
            static const QRegularExpression kRefValue(QStringLiteral("(\\bref\\.?\\s*:).*$"),
                                                      QRegularExpression::CaseInsensitiveOption);
            kept << QString(first).replace(kRefValue, QStringLiteral("\\1 ") + mask());
        }
    }
    const QString single = kept.join(QLatin1Char(' ')).simplified();
    return clean(single, secrets);
}

bool hasIdentifier(const QString& text, const QStringList& secrets)
{
    return clean(text, secrets) != text;
}

} // namespace StatementSanitizer
