#include "LabelNormalizer.h"

#include <QChar>
#include <QLatin1Char>
#include <QLatin1String>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <array>
#include <cstdint>

namespace {

// Bank-transport boilerplate: words that describe *how* the money moved, not
// *who* it moved to. Every one of these appears in labels from more than one
// merchant, so keeping them would make unrelated rows look similar to each
// other (and to the k-NN rung in particular).
//
// Deliberately NOT in this list, even though they look like candidates:
//   - "CREDIT" / "DEBIT" — the CSV already has separate debit/credit columns,
//     so in a *label* these are almost always part of a bank's name
//     ("CREDIT AGRICOLE", "CREDIT MUTUEL"), which is real merchant content.
//   - "COTISATION" / "FRAIS" — these ARE the merchant content for bank-fee
//     rows ("cotisation carte bancaire"), which is a category of its own.
//
// A label made only of these words gets no key (#40): before, normalize()
// ran again without this filter, and every such label ("Card payment 1234:
// H&M", "VIREMENT EN VOTRE FAVEUR") shared the key of its boilerplate.
//
// The words of the only template so far (the Sumeria export) are English:
// "Card payment 1234: …", "Payment received from …", "Transfer to …",
// "Direct debit: …" (#55).
const QSet<QString>& noiseTokens()
{
    static const QSet<QString> kNoise = {
        // Instrument / transport
        QStringLiteral("CARTE"),        QStringLiteral("CB"),
        QStringLiteral("CARD"),         QStringLiteral("ACHAT"),
        QStringLiteral("ACHATS"),       QStringLiteral("PAIEMENT"),
        QStringLiteral("PAIEMENTS"),    QStringLiteral("PAYMENT"),
        QStringLiteral("VIR"),          QStringLiteral("VIRT"),
        QStringLiteral("VIREMENT"),     QStringLiteral("VIREMENTS"),
        QStringLiteral("SEPA"),         QStringLiteral("PRLV"),
        QStringLiteral("PRELEVEMENT"),  QStringLiteral("PRELEVEMENTS"),
        QStringLiteral("PRELVT"),       QStringLiteral("RETRAIT"),
        QStringLiteral("RETRAITS"),     QStringLiteral("DAB"),
        QStringLiteral("GAB"),          QStringLiteral("DISTRIBUTEUR"),
        QStringLiteral("CHEQUE"),       QStringLiteral("CHQ"),
        QStringLiteral("TIP"),          QStringLiteral("TPE"),
        QStringLiteral("REMISE"),       QStringLiteral("DEPOT"),
        QStringLiteral("COMMERCE"),     QStringLiteral("ELECTRONIQUE"),
        QStringLiteral("SANS"),         QStringLiteral("CONTACT"),
        QStringLiteral("INTERNET"),
        // Reference / bookkeeping scaffolding
        QStringLiteral("REF"),          QStringLiteral("REFERENCE"),
        QStringLiteral("MANDAT"),       QStringLiteral("RUM"),
        QStringLiteral("ICS"),          QStringLiteral("ECH"),
        QStringLiteral("ECHEANCE"),     QStringLiteral("DATE"),
        QStringLiteral("NUM"),          QStringLiteral("NO"),
        QStringLiteral("OPERATION"),    QStringLiteral("OPERATIONS"),
        QStringLiteral("TRANSACTION"),  QStringLiteral("FACTURE"),
        QStringLiteral("FACT"),         QStringLiteral("EUR"),
        QStringLiteral("EUROS"),        QStringLiteral("USD"),
        // French grammatical filler
        QStringLiteral("DE"),           QStringLiteral("DES"),
        QStringLiteral("DU"),           QStringLiteral("LA"),
        QStringLiteral("LE"),           QStringLiteral("LES"),
        QStringLiteral("AU"),           QStringLiteral("AUX"),
        QStringLiteral("EN"),           QStringLiteral("ET"),
        QStringLiteral("PAR"),          QStringLiteral("POUR"),
        QStringLiteral("SUR"),          QStringLiteral("AVEC"),
        QStringLiteral("VOTRE"),        QStringLiteral("VOS"),
        QStringLiteral("NOTRE"),        QStringLiteral("FAVEUR"),
        QStringLiteral("EMIS"),         QStringLiteral("RECU"),
        QStringLiteral("MR"),           QStringLiteral("MME"),
        QStringLiteral("MLLE"),
        // English transport words of the Sumeria export (#55). "DIRECT
        // DEBIT" goes as a pair only (normalizeOnce()): alone, DEBIT can be
        // a name, as CREDIT is.
        QStringLiteral("TRANSFER"),     QStringLiteral("TRANSFERS"),
        QStringLiteral("RECEIVED"),     QStringLiteral("SENT"),
        QStringLiteral("FROM"),         QStringLiteral("TO"),
        QStringLiteral("WITHDRAWAL"),   QStringLiteral("ATM"),
        QStringLiteral("INTERNAL"),     QStringLiteral("EXTERNAL"),
    };
    return kNoise;
}

// Month names, French and English (#55): "RENT MARCH" and "RENT APRIL" are
// one merchant. A month name is a name only when the label has no other one
// ("MARS", "APRIL": Pass::MonthNames), and a month next to a day or a year
// ("05 MARS", "MARS 26", "MARS 2026") is a date in every pass.
const QSet<QString>& monthNames()
{
    static const QSet<QString> kMonths = {
        QStringLiteral("JANVIER"),      QStringLiteral("FEVRIER"),
        QStringLiteral("MARS"),         QStringLiteral("AVRIL"),
        QStringLiteral("MAI"),          QStringLiteral("JUIN"),
        QStringLiteral("JUILLET"),      QStringLiteral("AOUT"),
        QStringLiteral("SEPTEMBRE"),    QStringLiteral("OCTOBRE"),
        QStringLiteral("NOVEMBRE"),     QStringLiteral("DECEMBRE"),
        QStringLiteral("JANUARY"),      QStringLiteral("FEBRUARY"),
        QStringLiteral("MARCH"),        QStringLiteral("APRIL"),
        QStringLiteral("MAY"),          QStringLiteral("JUNE"),
        QStringLiteral("JULY"),         QStringLiteral("AUGUST"),
        QStringLiteral("SEPTEMBER"),    QStringLiteral("OCTOBER"),
        QStringLiteral("NOVEMBER"),     QStringLiteral("DECEMBER"),
    };
    return kMonths;
}

// Letters that NFD does not split (#55): "CŒUR" must be "COEUR". Applied
// after stripAccents(), before the split into tokens.
QString foldLigatures(const QString& value)
{
    struct Ligature {
        char16_t    letter;
        const char* replacement;
    };
    static constexpr std::array<Ligature, 12> kLigatures = {{
        {u'\u0152', "OE"}, {u'\u0153', "oe"}, {u'\u00C6', "AE"}, {u'\u00E6', "ae"},
        {u'\u00DF', "ss"}, {u'\u1E9E', "SS"}, {u'\uFB00', "ff"}, {u'\uFB01', "fi"},
        {u'\uFB02', "fl"}, {u'\uFB03', "ffi"}, {u'\uFB04', "ffl"}, {u'\uFB06', "st"},
    }};
    QString result;
    result.reserve(value.size());
    for (const QChar character : value) {
        // A loop, not std::find_if: the iterator of a std::array is a pointer
        // with libstdc++ but a class with MSVC.
        const char* replacement = nullptr;
        for (const Ligature& entry : kLigatures) {
            if (character == QChar(entry.letter)) {
                replacement = entry.replacement;
                break;
            }
        }
        if (replacement != nullptr) {
            result.append(QLatin1String(replacement));
        } else {
            result.append(character);
        }
    }
    return result;
}

// Shortest token worth keeping. One-character tokens ("M", "D", "X") are
// always scaffolding, never merchant content.
constexpr int kMinTokenLength = 2;
// A token that contains digits and is no longer than this is a reference
// number, a date fragment or a truncated card number ("12", "4972", "T3"),
// never a merchant name — except in the second pass (ShortCodes), for a label
// that has no other name: "AS24", "O2" (#40).
constexpr int kMaxShortDigitToken = 6;
// Ratio above which a mixed letters+digits token counts as an identifier
// rather than a name ("A1B2C3D4" → dropped, "M6" → dropped by length,
// "CARREFOUR2" → kept).
constexpr double kMaxDigitRatio = 0.5;
// A run of at least this many hexadecimal characters with a digit is a
// reference (a part of a UUID, a hash), not a name: "1A2B3C4D".
constexpr int kMinHexReferenceLength = 8;
// A masked card number has at least this many "X" before its digits:
// "XXXXXXXXXXXX4972" (#55).
constexpr int kMinMaskLength = 2;
// A day or a year next to a month name makes a date: "05 MARS", "MARS 2026".
constexpr int kMaxDayDigits = 2;
constexpr int kYearLength = 4;
constexpr int kFirstYear = 1900;
constexpr int kLastYear = 2099;

// Which tokens a pass keeps. A pass runs only when the passes before it
// found no name.
enum class Pass : std::uint8_t {
    Strict,     // no short code, no month name: "T3", "4972B" are references
    ShortCodes, // short codes too: "AS24", "O2" are names
    MonthNames, // month names too: "MARS", "APRIL" are names
};

// A UUID ("a1b2c3d4-0000-4000-8000-000000000001") is a reference. Its groups
// of four look like short codes or names ("4E8B", "BCDE"), so it goes as a
// whole, before the split into tokens (#40).
QString withoutUuids(const QString& value)
{
    if (!value.contains(QLatin1Char('-'))) {
        return value; // no UUID: the common case, without the regular expression
    }
    static const QRegularExpression kUuid(QStringLiteral(
        "\\b[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\\b"));
    QString result = value;
    result.replace(kUuid, QStringLiteral(" "));
    return result;
}

// The text before the first reference ("External ref.: …", "Mandate
// ref.: …"; the word before "ref", "End-to-end" too, is a part of the
// reference). The description of an imported row is its name, then its
// reference block (SumeriaCcpCsvExtractionTemplate.cpp), so a reference is
// all that the fallback to the description can add: never a name (#40).
// The fallback runs only when the name has only boilerplate, so the word
// before "ref" is never the name of the row.
QString beforeReferences(const QString& value)
{
    static const QRegularExpression kReference(QStringLiteral("(?:\\b[\\w-]+\\s+)?\\bref\\.?\\s*:"),
                                               QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = kReference.match(value);
    return match.hasMatch() ? value.left(match.capturedStart()) : value;
}

// True if `token` can be the day or the year of a date next to a month name:
// "05" ("05 MARS"), "26" ("MARS 26"), "2026" ("MARS 2026"). The tokens have
// only letters and digits, so a number has no sign.
bool isDayOrYear(const QString& token)
{
    bool isNumber = false;
    const int value = token.toInt(&isNumber);
    if (!isNumber) {
        return false;
    }
    return token.size() <= kMaxDayDigits
           || (token.size() == kYearLength && value >= kFirstYear && value <= kLastYear);
}

// Split on anything that isn't a letter or a digit, uppercasing as we go.
// Accents and ligatures are already gone by the time this runs. An "&"
// between two letters or digits stays in its token, so "H&M" and "C&A" are
// two names, not four letters that the length rule drops (#40).
QStringList rawTokens(const QString& value)
{
    QStringList tokens;
    QString current;
    current.reserve(value.size());

    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar character = value.at(i);
        if (character.isLetterOrNumber()) {
            current.append(character.toUpper());
        } else if (character == QLatin1Char('&') && !current.isEmpty() && i + 1 < value.size()
                   && value.at(i + 1).isLetterOrNumber()) {
            current.append(character);
        } else if (!current.isEmpty()) {
            tokens.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty()) {
        tokens.append(current);
    }
    return tokens;
}

// True if `token` looks like an identifier (reference, card fragment, date
// piece, amount) rather than a name. Applied regardless of the noise-word
// filter — a reference number is never merchant content.
bool isIdentifierToken(const QString& token, Pass pass)
{
    if (token.size() < kMinTokenLength) {
        return true;
    }

    int digits = 0;
    int masks = 0;
    int hex = 0;
    for (const QChar character : token) {
        if (character.isDigit()) {
            ++digits;
        } else if (character == QLatin1Char('X')) {
            ++masks;
        }
        if (character.isDigit() || (character >= QLatin1Char('A') && character <= QLatin1Char('F'))) {
            ++hex;
        }
    }
    if (digits == 0) {
        return false;
    }
    if (digits == token.size()) {
        return true; // pure number
    }
    if (masks >= kMinMaskLength && digits + masks == token.size()) {
        return true; // a masked card number: "XXXXXXXXXXXX4972"
    }
    if (token.size() >= kMinHexReferenceLength && hex == token.size()) {
        return true; // a part of a UUID or a hash: "A1B2C3D4"
    }
    if (pass == Pass::Strict && token.size() <= kMaxShortDigitToken) {
        return true; // short and partly numeric — "T3", "REF12", "4972B"
    }
    return static_cast<double>(digits) / static_cast<double>(token.size()) > kMaxDigitRatio;
}

// One normalization pass. The noise words always go: a label with only noise
// words gets an empty key.
Predict::NormalizedLabel normalizeOnce(const QString& value, Pass pass)
{
    Predict::NormalizedLabel result;
    if (value.trimmed().isEmpty()) {
        return result;
    }

    const QStringList tokens = rawTokens(withoutUuids(foldLigatures(Predict::LabelNormalizer::stripAccents(value))));
    for (qsizetype i = 0; i < tokens.size(); ++i) {
        const QString& token = tokens.at(i);
        if (isIdentifierToken(token, pass) || noiseTokens().contains(token)) {
            continue;
        }
        if (monthNames().contains(token)) {
            const bool date = (i > 0 && isDayOrYear(tokens.at(i - 1)))
                              || (i + 1 < tokens.size() && isDayOrYear(tokens.at(i + 1)));
            if (pass != Pass::MonthNames || date) {
                continue;
            }
        }
        // "Direct debit: …" (#55): the pair is boilerplate, each word alone
        // can be a name. The pair is found in the kept tokens, so that
        // "DIRECT 12/03 DEBIT" is a pair too and normalize() of a key gives
        // the key.
        if (token == QLatin1String("DEBIT") && !result.tokens.isEmpty()
            && result.tokens.constLast() == QLatin1String("DIRECT")) {
            result.tokens.removeLast();
            continue;
        }
        result.tokens.append(token);
    }

    result.key = result.tokens.join(QLatin1Char(' '));
    return result;
}

} // namespace

namespace Predict::LabelNormalizer {

QString stripAccents(const QString& value)
{
    // NFD splits "é" into "e" + U+0301 COMBINING ACUTE ACCENT; dropping every
    // non-spacing mark then leaves the base letters.
    const QString decomposed = value.normalized(QString::NormalizationForm_D);

    QString result;
    result.reserve(decomposed.size());
    for (const QChar character : decomposed) {
        if (character.category() != QChar::Mark_NonSpacing) {
            result.append(character);
        }
    }
    return result;
}

bool isNoiseToken(const QString& token)
{
    const QString upper = token.toUpper();
    return noiseTokens().contains(upper) || monthNames().contains(upper);
}

NormalizedLabel normalize(const QString& name, const QString& description)
{
    // Six attempts, strictest first: three passes of the name, then of the
    // description before its first reference. Each fallback only runs when
    // the previous one produced an empty key, so the common case costs
    // exactly one pass. The second pass keeps the short codes ("AS24", "O2")
    // and the third the month names ("MARS", "APRIL"): a label whose only
    // name is one. A label with only noise words gets an empty key — no
    // suggestion, no rule — rather than the key of its boilerplate, which
    // every such row would share (#40).
    const auto firstKey = [](const QString& text) {
        for (const Pass pass : {Pass::Strict, Pass::ShortCodes, Pass::MonthNames}) {
            NormalizedLabel result = normalizeOnce(text, pass);
            if (!result.isEmpty()) {
                return result;
            }
        }
        return NormalizedLabel();
    };
    NormalizedLabel result = firstKey(name);
    if (result.isEmpty()) {
        result = firstKey(beforeReferences(description));
    }
    return result;
}

QStringList charNgrams(const QString& key)
{
    QStringList grams;
    if (key.isEmpty()) {
        return grams;
    }
    // Spaces are kept on purpose: they carry the word boundary, so "R M" (the
    // end of CARREFOUR + start of MARKET) is a real feature rather than noise.
    if (key.size() <= kNgramSize) {
        grams.append(key);
        return grams;
    }

    const qsizetype last = key.size() - kNgramSize;
    grams.reserve(last + 1);
    for (qsizetype i = 0; i <= last; ++i) {
        grams.append(key.mid(i, kNgramSize));
    }
    return grams;
}

double tokenJaccard(const QStringList& lhs, const QStringList& rhs)
{
    if (lhs.isEmpty() || rhs.isEmpty()) {
        return 0.0; // "both had nothing" is not a match
    }

    const QSet<QString> left(lhs.cbegin(), lhs.cend());
    const QSet<QString> right(rhs.cbegin(), rhs.cend());

    qsizetype intersection = 0;
    for (const QString& token : left) {
        if (right.contains(token)) {
            ++intersection;
        }
    }
    const qsizetype unionSize = left.size() + right.size() - intersection;
    if (unionSize <= 0) {
        return 0.0;
    }
    return static_cast<double>(intersection) / static_cast<double>(unionSize);
}

} // namespace Predict::LabelNormalizer
