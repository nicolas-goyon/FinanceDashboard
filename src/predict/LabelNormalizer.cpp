#include "LabelNormalizer.h"

#include <QChar>
#include <QSet>
#include <QString>
#include <QStringList>

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
// Over-stripping is guarded against rather than avoided: normalize() re-runs
// without this filter when applying it leaves nothing behind (see below), so
// a label made entirely of these words still produces a usable key.
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
    };
    return kNoise;
}

// Shortest token worth keeping. One-character tokens ("M", "D", "X") are
// always scaffolding, never merchant content.
constexpr int kMinTokenLength = 2;
// A token that contains digits and is no longer than this is a reference
// number, a date fragment or a truncated card number ("12", "4972", "T3"),
// never a merchant name.
constexpr int kMaxShortDigitToken = 6;
// Ratio above which a mixed letters+digits token counts as an identifier
// rather than a name ("A1B2C3D4" → dropped, "M6" → dropped by length,
// "CARREFOUR2" → kept).
constexpr double kMaxDigitRatio = 0.5;

// Split on anything that isn't a letter or a digit, uppercasing as we go.
// Accents are already gone by the time this runs.
QStringList rawTokens(const QString& value)
{
    QStringList tokens;
    QString current;
    current.reserve(value.size());

    for (const QChar character : value) {
        if (character.isLetterOrNumber()) {
            current.append(character.toUpper());
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
// filter — a reference number is never merchant content, so unlike the
// stop-word list this one has no fallback that re-admits it.
bool isIdentifierToken(const QString& token)
{
    if (token.size() < kMinTokenLength) {
        return true;
    }

    int digits = 0;
    for (const QChar character : token) {
        if (character.isDigit()) {
            ++digits;
        }
    }
    if (digits == 0) {
        return false;
    }
    if (digits == token.size()) {
        return true; // pure number
    }
    if (token.size() <= kMaxShortDigitToken) {
        return true; // short and partly numeric — "T3", "REF12", "4972B"
    }
    return static_cast<double>(digits) / static_cast<double>(token.size()) > kMaxDigitRatio;
}

// One normalization pass. `applyNoiseFilter` off means "keep the transport
// words too" — the fallback used when filtering leaves nothing behind.
Predict::NormalizedLabel normalizeOnce(const QString& value, bool applyNoiseFilter)
{
    Predict::NormalizedLabel result;
    if (value.trimmed().isEmpty()) {
        return result;
    }

    const QStringList tokens = rawTokens(Predict::LabelNormalizer::stripAccents(value));
    for (const QString& token : tokens) {
        if (isIdentifierToken(token)) {
            continue;
        }
        if (applyNoiseFilter && Predict::LabelNormalizer::isNoiseToken(token)) {
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
    return noiseTokens().contains(token.toUpper());
}

NormalizedLabel normalize(const QString& name, const QString& description)
{
    // Four attempts, most aggressive first. Each fallback only runs when the
    // previous one produced an empty key, so the common case costs exactly
    // one pass — and a row whose label is *entirely* boilerplate still ends
    // up with a key rather than silently dropping out of the predictor.
    NormalizedLabel result = normalizeOnce(name, true);
    if (!result.isEmpty()) {
        return result;
    }

    result = normalizeOnce(name, false);
    if (!result.isEmpty()) {
        return result;
    }

    result = normalizeOnce(description, true);
    if (!result.isEmpty()) {
        return result;
    }

    return normalizeOnce(description, false);
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
