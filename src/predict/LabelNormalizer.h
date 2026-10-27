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
// meaningful as the predictor's front end. Its stop-word list is tuned for
// bank statement labels: French ones, and the English ones of the Sumeria
// export (#55).
//
// A label with only boilerplate (transport words, references) gets an empty
// key (#40): no suggestion and no rule, rather than a key that every such
// label shares. Short names keep their own key: "H&M", "C&A". A short name
// with a digit ("AS24", "O2") is the name when the label has no other name;
// next to another name it is taken for a reference, as "T3" and "4972B" are:
// "AS24 LYON" gives "LYON". A month name is not part of the key ("RENT
// MARCH" gives "RENT"), except when the label has no other name ("MARS",
// "APRIL"); next to a day or a year ("05 MARS", "MARS 2026") it is a date.
// Limit: a label with only boilerplate and a month ("FACTURE MARS") gets
// the month as its key. Ligatures are folded ("CŒUR" is "COEUR"), and masked
// card numbers ("XXXXXXXXXXXX4972") and UUIDs are references (#55). A key
// has no direction words ("TRANSFER TO", "RECEIVED FROM"): a rule covers
// the debits and the credits of a name (TODO.md).
//
// **Rule patterns are keys.** The patterns of the stored category rules
// (Data::CategoryRule) are keys of normalize(). So a change of the keys (a
// new noise word, another rule for the tokens) needs a new schema step that
// normalizes the stored patterns again, as step 6 does
// (Database::migrateTo6()). Such a step calls normalize() of the build that
// runs it, and it needs normalize() of a key to give the same key: keep that
// true.

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
// `name` normalizes to nothing at all, and only its text before the first
// reference ("Internal ref.: …", with the word before "ref"). A label with no
// merchant content in either gets an empty key (#40).
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
// "PRLV", "TRANSFER", …) or a month name, rather than merchant content (a
// month name is a name only when the label has no other one: see
// normalize()). Exposed for tests and for the docs' worked examples;
// normalize() applies it already.
[[nodiscard]] bool isNoiseToken(const QString& token);

// Length of the character n-grams produced by charNgrams().
constexpr int kNgramSize = 3;

} // namespace LabelNormalizer

} // namespace Predict

#endif // FINANCEDASHBOARD_PREDICT_LABELNORMALIZER_H
