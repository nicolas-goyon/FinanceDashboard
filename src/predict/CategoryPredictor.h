#ifndef FINANCEDASHBOARD_PREDICT_CATEGORYPREDICTOR_H
#define FINANCEDASHBOARD_PREDICT_CATEGORYPREDICTOR_H

#include "models/CategoryRule.h"
#include "models/Transaction.h"
#include "predict/LabelNormalizer.h"

#include <QDate>
#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

// ── Predict::CategoryPredictor ────────────────────────────────────────────────
//
// Suggests a category/subcategory for an incoming statement row, learned from
// the user's own already-categorized transactions. Full design rationale in
// docs/category-prediction.md — this header documents the contract.
//
// **Why not "real" machine learning.** One user, one bank, a few hundred
// transactions, and a brand-new category the moment the user invents one. A
// trained model needs far more data than that and re-trains slowly; what
// actually wins here is a memory of what the user already did, plus enough
// fuzziness to generalize one example to the next month's variant of the same
// label. So this is a **cascade**, tried highest-precision first, and every
// rung is exact-computable from the data with no iterative fitting:
//
//   1. "rule"    — a user-authored Data::CategoryRule matched the key.
//                  Confidence 1.0. Learns from one click, permanently.
//   2. "exact"   — this exact merchant key has been categorized before.
//                  Learns from a single example.
//   3. "fuzzy"   — no exact hit, but a known key is ≥ kFuzzyMinSimilarity
//                  similar by token-set Jaccard ("CARREFOUR MARKET LYON" vs
//                  "CARREFOUR MARKET").
//   4. "similar" — TF-IDF weighted k-nearest-neighbours over word tokens AND
//                  character n-grams. The generalizing rung: a token like
//                  "SNCF" seen once transfers to any new label containing it,
//                  and n-grams survive concatenation ("SNCFCONNECT").
//
// Two cross-cutting adjustments are applied to whichever rung answered:
//   - **sign gating** — a category only ever seen on debits is never
//     suggested with full confidence for an incoming credit (and vice versa),
//     which kills a whole class of embarrassing suggestions ("Groceries" for
//     an incoming transfer).
//   - **recurrence boost** — same key, same amount (±kRecurrenceTolerance),
//     spaced about a month apart: rent, salary and subscriptions are a large
//     share of rows and are perfectly predictable, so they go to
//     kRecurrenceConfidence.
//
// **Confidence is calibrated against the review screen's own thresholds**
// (danger < 0.5, warning < 0.8, success ≥ 0.8 — see Fmt::confidenceVariant()).
// For the memory rungs it is `purity × maturity × similarity`, where
// purity = how consistently this key got the winning label and
// maturity = n / (n + kMaturityBias). That means: seen once → 0.67 (amber,
// "check this"), twice → 0.80 (green), three times → 0.86. It commits after
// two confirmations rather than one, and it *drops back* automatically when
// the user recategorizes, because purity falls.
//
// **Training is just indexing.** train() is a single pass over the examples —
// no iteration, no convergence, no persisted model file. A few thousand rows
// is single-digit milliseconds, so callers rebuild rather than maintaining
// incremental updates; that also means corrections made anywhere (the import
// review screen, AccountTransactionsPage's edit action) feed straight back in
// with no extra plumbing, which is what PROJECT.md § Category System asks for.
//
// **Ties (#51).** The same data gives the same suggestion in every run: sums
// of doubles add their terms in a fixed order (a QHash has another order in
// each run), scores are compared at a resolution of 1e-9, and a tie goes to
// the label of the most recent example, in every rung (the latest habit of
// the user, maintainer decision of 2026-10-21). train() puts the examples in
// date order for that. Two rules of the same priority and pattern length
// that both match: the rule made last (the higher id) applies.
//
// **Nothing here is persisted except the rules.** Prediction and confidence
// are review-screen-only concepts (see Data::ImportedTransaction::confidence
// and docs/statement-import.md) — this class never writes to the database.
// It is UI-independent: no QObject, no QML_ELEMENT. SnapshotImportController
// owns an instance and does the Qt/QML plumbing.

namespace Predict {

// ── Value types ───────────────────────────────────────────────────────────────

// One already-categorized row the predictor learns from.
struct Example {
    QString     key;         // Predict::LabelNormalizer::normalize() output
    QStringList tokens;      // the same, pre-split
    QString     category;
    QString     subcategory; // may be empty
    double      signedAmount = 0.0; // credit positive, debit negative
    QDate       date;        // train() orders the examples by it: the most recent wins a tie
};

// What the predictor came back with for one row.
struct Prediction {
    QString category;
    QString subcategory;
    double  confidence = 0.0;  // 0.0 means "not scored", never "0% sure"
    QString rung;              // "", "rule", "exact", "fuzzy" or "similar"
    QString reason;            // one line of plain English for the review screen
    QString key;               // the merchant key this row normalized to

    [[nodiscard]] bool isEmpty() const { return category.isEmpty(); }
};

// Result of evaluate() — a real accuracy number measured on the user's own
// data, so the constants below can be tuned against something instead of
// guessed at. See evaluate() for the methodology and its one caveat.
struct Evaluation {
    int    total           = 0; // labeled examples with a usable key
    int    attempted       = 0; // of those, how many got any suggestion at all
    int    categoryCorrect = 0; // right category (subcategory ignored)
    int    pairCorrect     = 0; // right category AND right subcategory
    double meanConfidence  = 0.0; // over attempted predictions only
    QMap<QString, int> attemptsByRung;
    QMap<QString, int> correctByRung; // category-level correctness per rung
};

// ── Tuning constants ──────────────────────────────────────────────────────────
// Every one of these is a judgement call; evaluate() is how you check them
// against real data rather than arguing about them.

// n / (n + kMaturityBias) — see the confidence discussion above.
inline constexpr double kMaturityBias = 0.5;
// Below this token-set Jaccard, two keys are not "the same merchant".
inline constexpr double kFuzzyMinSimilarity = 0.6;
// Neighbours considered by the "similar" rung.
inline constexpr int kNeighbourCount = 5;
// Cosine below this is noise, not a neighbour. Low on purpose: TF-IDF cosine
// over short sparse labels is small in absolute terms even for a genuine
// match (one shared rare token between a 2-token and a 4-token label lands
// around 0.2), so a "looks high" threshold like 0.5 would switch this rung
// off almost entirely. kSimilarityFullCredit below is what keeps a weak match
// from turning into a confident suggestion.
inline constexpr double kNeighbourMinSimilarity = 0.15;
// Cosine at or above which the "similar" rung stops discounting for
// closeness. Between kNeighbourMinSimilarity and this, confidence scales
// linearly — so a barely-above-threshold neighbour lands in the review
// screen's red band ("guess, check it") rather than its green one.
inline constexpr double kSimilarityFullCredit = 0.5;
// A generalization must never outrank a memory, so the "similar" rung is
// capped below what two confirmed sightings of an exact key would score.
inline constexpr double kSimilarConfidenceCeiling = 0.75;
// Character n-grams are supporting evidence, not primary — half a token's weight.
inline constexpr double kNgramWeight = 0.5;
// Amounts within this fraction of each other count as "the same" for recurrence.
inline constexpr double kRecurrenceTolerance = 0.02;
// Confirmed monthly recurrence is about as certain as this gets short of a rule.
inline constexpr double kRecurrenceConfidence = 0.95;
// Inclusive day-gap window that counts as "about a month apart".
inline constexpr int kRecurrenceMinGapDays = 25;
inline constexpr int kRecurrenceMaxGapDays = 35;
// A category needs at least this many same-sign sightings before its sign is
// treated as established (one debit does not prove a category is debit-only).
inline constexpr int kSignGateMinExamples = 3;
// Ceiling applied when the sign contradicts everything we've seen.
inline constexpr double kSignMismatchCeiling = 0.4;
// If the *category* is this much purer than the best (category, subcategory)
// pair, suggest the category alone rather than guessing the subcategory.
inline constexpr double kPairAmbiguityMargin = 0.15;
// Below this, return nothing at all rather than a suggestion the user has to
// undo — an empty cell is cheaper to fix than a wrong one.
inline constexpr double kMinConfidenceToSuggest = 0.2;

// ── The predictor ─────────────────────────────────────────────────────────────

class CategoryPredictor
{
public:
    CategoryPredictor() = default;

    // Rebuild the whole index from scratch. Examples with an empty category
    // or an empty key are ignored (they carry no signal). The others are put
    // in date order (an invalid date first, the given order on one date): a
    // tie goes to the most recent one (#51). Rules are kept in match order:
    // higher priority first, then longer pattern first, so a specific rule
    // beats a general one that also matches.
    void train(const QVector<Example>& examples, const QVector<Data::CategoryRule>& rules);

    // Drops every example, rule and index. isTrained() is false afterwards.
    void clear();

    [[nodiscard]] bool isTrained() const { return !m_examples.isEmpty() || !m_rules.isEmpty(); }
    [[nodiscard]] int  exampleCount() const { return static_cast<int>(m_examples.size()); }
    [[nodiscard]] int  ruleCount() const { return static_cast<int>(m_rules.size()); }
    // Distinct merchant keys learned — a better "how much does it know"
    // number for the UI than exampleCount(), which double-counts repeats.
    [[nodiscard]] int  distinctKeyCount() const { return static_cast<int>(m_keyIndex.size()); }

    // The main entry point. `signedAmount` is credit - debit (so a debit is
    // negative); `date` may be invalid, which only disables the recurrence
    // boost. Returns an empty Prediction (category empty, confidence 0.0)
    // when nothing cleared kMinConfidenceToSuggest — callers should leave the
    // row blank in that case, not fill in a guess.
    [[nodiscard]] Prediction predict(const QString& name, const QString& description,
                                      double signedAmount, const QDate& date) const;

    // Leave-one-out accuracy over the training set: every example is
    // predicted with itself excluded from every rung, then compared against
    // its real label. Rules are deliberately NOT applied — a rule the user
    // created *from* one of these rows would score itself, so this measures
    // the learned part only.
    //
    // Caveat: the IDF weights are still computed over the full set, so the
    // "similar" rung sees a trace of the held-out row. The effect is tiny at
    // this corpus size and removing it would mean rebuilding the index N
    // times; documented rather than fixed.
    [[nodiscard]] Evaluation evaluate() const;

    // Convenience conversions from the persisted row shape. Rows with an
    // empty category are dropped by examplesFromTransactions() — an
    // uncategorized transaction is not a training example.
    [[nodiscard]] static Example exampleFrom(const Data::Transaction& transaction);
    [[nodiscard]] static QVector<Example> examplesFromTransactions(const QVector<Data::Transaction>& transactions);

private:
    // A (category, subcategory) pair chosen from a set of examples, plus the
    // bookkeeping the confidence formula needs.
    struct LabelChoice {
        QString category;
        QString subcategory;
        double  purity      = 0.0;
        int     support     = 0;    // examples backing the winning label
        int     total       = 0;    // examples considered
        bool    droppedSubcategory = false; // pair was ambiguous, category wasn't
    };

    // Shared body of predict()/evaluate(). `excludeIndex` hides one example
    // from every rung (leave-one-out); `useRules` turns the rule rung off.
    [[nodiscard]] Prediction predictFor(const NormalizedLabel& label, double signedAmount,
                                         const QDate& date, int excludeIndex, bool useRules) const;

    [[nodiscard]] Prediction matchRule(const NormalizedLabel& label) const;
    [[nodiscard]] Prediction matchExactKey(const NormalizedLabel& label, int excludeIndex) const;
    [[nodiscard]] Prediction matchFuzzyKey(const NormalizedLabel& label, int excludeIndex) const;
    [[nodiscard]] Prediction matchNeighbours(const NormalizedLabel& label, int excludeIndex) const;

    // Majority label over `indices`, with the pair-ambiguity rule applied.
    [[nodiscard]] LabelChoice chooseLabel(const QVector<int>& indices, int excludeIndex) const;

    // Feature vector for a key: "w:TOKEN" at weight 1, "g:NGR" at kNgramWeight.
    [[nodiscard]] static QHash<QString, double> featuresFor(const NormalizedLabel& label);
    [[nodiscard]] double inverseDocumentFrequency(const QString& feature) const;

    // False when `category` has been seen at least kSignGateMinExamples times
    // and never with this sign.
    [[nodiscard]] bool signIsPlausible(const QString& category, double signedAmount) const;
    // Same key, same amount, roughly monthly spacing, at least twice before.
    [[nodiscard]] bool looksRecurring(const QString& key, double signedAmount,
                                       const QDate& date, int excludeIndex) const;

    QVector<Example>            m_examples;
    QVector<Data::CategoryRule> m_rules;      // pre-sorted into match order

    QHash<QString, QVector<int>> m_keyIndex;  // merchant key   -> example indices
    QHash<QString, QVector<int>> m_postings;  // feature        -> example indices
    QHash<QString, int>          m_documentFrequency; // feature -> #examples containing it
    QVector<QHash<QString, double>> m_vectors;        // per-example raw feature weights
    QVector<double>              m_norms;             // per-example L2 norm of tf-idf
    // category -> (credits seen, debits seen), for signIsPlausible().
    QHash<QString, QPair<int, int>> m_categorySigns;
};

} // namespace Predict

#endif // FINANCEDASHBOARD_PREDICT_CATEGORYPREDICTOR_H
