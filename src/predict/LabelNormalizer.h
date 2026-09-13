#ifndef FINANCEDASHBOARD_PREDICT_LABELNORMALIZER_H
#define FINANCEDASHBOARD_PREDICT_LABELNORMALIZER_H

#include <QString>
#include <QStringList>

// ── Predict::LabelNormalizer ──────────────────────────────────────────────────
//
// Turns a bank transaction label into a stable **merchant key** — the single
// most important step of category prediction (see
// docs/category-prediction.md). Bank labels carry a lot of per-transaction
// noise (card number, operation date, terminal id, city) around a small stable
// core:
//
//     "CARTE 12/03 CARREFOUR MARKET 4972 LYON"  →  "CARREFOUR MARKET LYON"
//     "VIR SEPA CARREFOUR MARKET"               →  "CARREFOUR MARKET"
//     "PRLV SEPA EDF  ref.: 1234567"            →  "EDF"
//
// Strip the noise and the "dumb" exact-label lookup already gets most rows
// right; leave it in and every row looks unique. Everything in
// CategoryPredictor operates on the output of this file — nothing else in the
// predictor ever sees a raw label.
//
// **Both sides must use this one function.** The stored transactions and the
// incoming statement rows have to be normalized identically or the two sides
// compute different keys for the same merchant and nothing ever matches —
// same "never re-derive it independently" rule as AccountHashing::compute()
// (src/utils/AccountHashing.h).
//
// Lives under src/predict/ rather than src/utils/ because it is only
// meaningful as the predictor's front end, and its stop-word list is tuned
// for French bank statement labels specifically.

namespace Predict {

// A raw label reduced to its stable core.
struct NormalizedLabel {
    QString     key;    // tokens joined by single spaces, e.g. "CARREFOUR MARKET"
                        // Empty only when the input had no usable content at all.
    QStringList tokens; // the same content pre-split, e.g. ["CARREFOUR", "MARKET"]

    [[nodiscard]] bool isEmpty() const { return key.isEmpty(); }
};

namespace LabelNormalizer {

// Unicode NFD decomposition with the combining marks dropped: "Crédit" →
// "Credit". Accents are inconsistent across bank exports (and across the
// user's own manual entries), so they must never be part of a key.
[[nodiscard]] QString stripAccents(const QString& value);

// The main entry point. `name` is Data::Transaction::name /
// Data::ImportedTransaction::name (already reduced by the import template's
// extractTransactionName()); `description` is only used as a fallback when
// `name` normalizes to nothing at all, so a row with an unusual label shape
// still gets *some* key instead of dropping out of the predictor entirely.
[[nodiscard]] NormalizedLabel normalize(const QString& name, const QString& description = QString());

// Character n-grams of `key` (n = kTrigramSize), with spaces kept so word
// boundaries still matter. Used as extra k-NN features: they survive
// concatenation and typos ("SNCFCONNECT" vs "SNCF CONNECT") that whole-token
// matching misses completely.
[[nodiscard]] QStringList charNgrams(const QString& key);

// Jaccard similarity of two token sets: |A ∩ B| / |A ∪ B|, in [0, 1].
// Duplicate tokens are ignored (set semantics). Two empty lists → 0.0, not
// 1.0 — "both had nothing" is not a match.
[[nodiscard]] double tokenJaccard(const QStringList& lhs, const QStringList& rhs);

// True if `token` is bank-transport boilerplate ("CARTE", "VIR", "SEPA",
// "PRLV", …) rather than merchant content. Exposed for tests and for the
// docs' worked examples; normalize() applies it already.
[[nodiscard]] bool isNoiseToken(const QString& token);

// Length of the character n-grams produced by charNgrams().
constexpr int kNgramSize = 3;

} // namespace LabelNormalizer

} // namespace Predict

#endif // FINANCEDASHBOARD_PREDICT_LABELNORMALIZER_H
