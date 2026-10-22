#include "CategoryPredictor.h"

#include "models/CategoryRule.h"
#include "models/Transaction.h"
#include "predict/LabelNormalizer.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include <QChar>
#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

namespace {

// (category, subcategory) pairs are counted under one hash key joined by an
// ASCII unit separator — a character that cannot occur in a user-typed
// category name, so the join is unambiguously reversible.
constexpr char16_t kPairSeparator = u'\x1f';

// n / (n + kMaturityBias): 1 sighting → 0.67, 2 → 0.80, 3 → 0.86, 10 → 0.95.
// Deliberately not 1 - 1/n (too harsh at n=1) and not a flat 1.0 (which would
// let a single accidental categorization masquerade as settled fact).
double maturity(int observations)
{
    if (observations <= 0) {
        return 0.0;
    }
    const auto count = static_cast<double>(observations);
    return count / (count + Predict::kMaturityBias);
}

QString joinPair(const QString& category, const QString& subcategory)
{
    return category + QChar(kPairSeparator) + subcategory;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — the two out-params
// mirror the (category, subcategory) order used everywhere else in this file
// and in Data::CategoryRule, so a swap isn't a realistic mistake; same
// convention as the table classes' insert() signatures.
void splitPair(const QString& pair, QString& category, QString& subcategory)
{
    const qsizetype separatorIndex = pair.indexOf(QChar(kPairSeparator));
    if (separatorIndex < 0) {
        category = pair;
        subcategory.clear();
        return;
    }
    category = pair.left(separatorIndex);
    subcategory = pair.mid(separatorIndex + 1);
}

// "Groceries" or "Groceries / Supermarket" — how a label is named in the
// human-readable reason strings shown on the review screen.
QString labelText(const QString& category, const QString& subcategory)
{
    if (subcategory.isEmpty()) {
        return category;
    }
    return category + QStringLiteral(" / ") + subcategory;
}

// ── Ties (#51) ────────────────────────────────────────────────────────────────
// The same data must give the same suggestion in every run. A QHash has
// another iteration order in each run (its seed is random), so:
//   - a sum of doubles adds its terms in a fixed order (sortedKeys()): the
//     order of the terms changes the last bits of the result;
//   - scores are compared at a resolution of 1e-9 (rankOf()), so two scores
//     that differ only in their last bits are a tie;
//   - a tie goes to the label of the most recent example (maintainer
//     decision of 2026-10-21: the latest habit of the user). train() puts
//     the examples in date order, so a higher index is a more recent example.

constexpr double kTieResolution = 1e9;

qint64 rankOf(double score)
{
    return std::llround(score * kTieResolution);
}

// The keys of `features`, sorted: a sum over them is the same in every run.
QStringList sortedKeys(const QHash<QString, double>& features)
{
    QStringList keys = features.keys();
    keys.sort();
    return keys;
}

// The votes for one label: their weight (a count, or a sum of similarities),
// and the index of the most recent example among them.
struct Tally {
    double weight = 0.0;
    int    latest = -1;
};

// The weight and the example index: each call names them (`similarity`,
// `index`), so a swap is not a realistic mistake.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void addVote(QHash<QString, Tally>& tallies, const QString& label, double weight, int index)
{
    Tally& tally = tallies[label];
    tally.weight += weight;
    tally.latest = std::max(tally.latest, index);
}

// The label with the highest weight; at equal weights (rankOf()), the label
// of the most recent example. Two labels never share an example, so the
// choice does not depend on the order of the QHash.
QString bestLabel(const QHash<QString, Tally>& tallies, double& bestWeight)
{
    QString bestKey;
    Tally best;
    for (auto it = tallies.constBegin(); it != tallies.constEnd(); ++it) {
        const qint64 rank = rankOf(it.value().weight);
        const qint64 bestRank = rankOf(best.weight);
        if (bestKey.isEmpty() || rank > bestRank || (rank == bestRank && it.value().latest > best.latest)) {
            bestKey = it.key();
            best = it.value();
        }
    }
    bestWeight = best.weight;
    return bestKey;
}

// True when `pattern` is `key` itself or a whole-token run inside it. Padding
// both with spaces is what makes it token-aware: "EDF" then matches
// "EDF PARIS" but not "REDFORD".
bool keyMatchesPattern(const QString& key, const QString& pattern)
{
    if (pattern.isEmpty() || key.isEmpty()) {
        return false;
    }
    if (key == pattern) {
        return true;
    }
    const QString paddedKey = QLatin1Char(' ') + key + QLatin1Char(' ');
    const QString paddedPattern = QLatin1Char(' ') + pattern + QLatin1Char(' ');
    return paddedKey.contains(paddedPattern);
}

// QLatin1String rather than QString: these are file-static, and a QString at
// static storage duration allocates during dynamic initialization (which
// cert-err58-cpp flags, since a throw there can't be caught). QLatin1String is
// constexpr, assigns straight into Prediction::rung, and compares against a
// QString without allocating.
constexpr auto kRungRule    = QLatin1String("rule");
constexpr auto kRungExact   = QLatin1String("exact");
constexpr auto kRungFuzzy   = QLatin1String("fuzzy");
constexpr auto kRungSimilar = QLatin1String("similar");

} // namespace

namespace Predict {

// ── Training ──────────────────────────────────────────────────────────────────

void CategoryPredictor::clear()
{
    m_examples.clear();
    m_rules.clear();
    m_keyIndex.clear();
    m_postings.clear();
    m_documentFrequency.clear();
    m_vectors.clear();
    m_norms.clear();
    m_categorySigns.clear();
}

void CategoryPredictor::train(const QVector<Example>& examples, const QVector<Data::CategoryRule>& rules)
{
    clear();

    // Rules first — they're independent of the examples and cheap.
    m_rules.reserve(rules.size());
    for (const Data::CategoryRule& rule : rules) {
        if (rule.pattern.isEmpty() || rule.category.isEmpty()) {
            continue; // an unusable rule would silently swallow every row
        }
        m_rules.append(rule);
    }
    // Match order: explicit priority first, then the more specific (longer)
    // pattern, so "CARREFOUR MARKET" is tried before a broader "CARREFOUR".
    // Two rules equal on both: the rule made last (the higher id) is tried
    // first, the same "most recent wins" as the ties of the examples (#51).
    // The pattern order is the last step, for rules without an id.
    std::sort(m_rules.begin(), m_rules.end(), [](const Data::CategoryRule& lhs, const Data::CategoryRule& rhs) {
        if (lhs.priority != rhs.priority) {
            return lhs.priority > rhs.priority;
        }
        if (lhs.pattern.size() != rhs.pattern.size()) {
            return lhs.pattern.size() > rhs.pattern.size();
        }
        if (lhs.id != rhs.id) {
            return lhs.id > rhs.id;
        }
        return lhs.pattern < rhs.pattern;
    });

    // Keep only examples that carry signal: a label we could normalize, and a
    // category the user actually chose.
    m_examples.reserve(examples.size());
    for (const Example& example : examples) {
        if (example.key.isEmpty() || example.category.isEmpty()) {
            continue;
        }
        m_examples.append(example);
    }
    // Date order (an invalid date first), and the given order on one date:
    // the index of an example tells how recent it is, for the ties (#51).
    std::stable_sort(m_examples.begin(), m_examples.end(),
                     [](const Example& lhs, const Example& rhs) { return lhs.date < rhs.date; });

    const int count = static_cast<int>(m_examples.size());
    m_vectors.resize(count);
    m_norms.resize(count);

    for (int i = 0; i < count; ++i) {
        const Example& example = m_examples.at(i);

        m_keyIndex[example.key].append(i);

        QPair<int, int>& signs = m_categorySigns[example.category];
        if (example.signedAmount > 0.0) {
            ++signs.first;
        } else if (example.signedAmount < 0.0) {
            ++signs.second;
        }

        NormalizedLabel label;
        label.key = example.key;
        label.tokens = example.tokens;

        QHash<QString, double> features = featuresFor(label);
        for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
            ++m_documentFrequency[it.key()];
            m_postings[it.key()].append(i);
        }
        m_vectors[i] = std::move(features);
    }

    // Norms need the finished document frequencies, so this is a second pass.
    // The terms are added in a fixed order (#51).
    for (int i = 0; i < count; ++i) {
        double sumOfSquares = 0.0;
        const QHash<QString, double>& vector = m_vectors.at(i);
        for (const QString& feature : sortedKeys(vector)) {
            const double weight = vector.value(feature) * inverseDocumentFrequency(feature);
            sumOfSquares += weight * weight;
        }
        m_norms[i] = std::sqrt(sumOfSquares);
    }
}

QHash<QString, double> CategoryPredictor::featuresFor(const NormalizedLabel& label)
{
    QHash<QString, double> features;
    for (const QString& token : label.tokens) {
        features[QStringLiteral("w:") + token] += 1.0;
    }
    for (const QString& gram : LabelNormalizer::charNgrams(label.key)) {
        features[QStringLiteral("g:") + gram] += kNgramWeight;
    }
    return features;
}

double CategoryPredictor::inverseDocumentFrequency(const QString& feature) const
{
    // Smoothed IDF, always ≥ 1 so a feature never contributes zero weight:
    // log((N + 1) / (df + 1)) + 1. A feature the corpus has never seen gets
    // the maximum weight, which correctly *lowers* the cosine similarity of a
    // query full of unknown words (it inflates the query norm while adding
    // nothing to any dot product).
    const auto total = static_cast<double>(m_examples.size());
    const auto frequency = static_cast<double>(m_documentFrequency.value(feature, 0));
    return std::log((total + 1.0) / (frequency + 1.0)) + 1.0;
}

// ── Prediction ────────────────────────────────────────────────────────────────

Prediction CategoryPredictor::predict(const QString& name, const QString& description,
                                        double signedAmount, const QDate& date) const
{
    const NormalizedLabel label = LabelNormalizer::normalize(name, description);
    return predictFor(label, signedAmount, date, -1, true);
}

Prediction CategoryPredictor::predictFor(const NormalizedLabel& label, double signedAmount,
                                           const QDate& date, int excludeIndex, bool useRules) const
{
    Prediction none;
    none.key = label.key;

    if (label.key.isEmpty()) {
        return none; // nothing to match on — leave the row blank
    }

    // The cascade: first rung with an answer wins. Ordering is precision
    // first, coverage last — see the class comment for why.
    Prediction prediction = useRules ? matchRule(label) : Prediction{};
    if (prediction.isEmpty()) {
        prediction = matchExactKey(label, excludeIndex);
    }
    if (prediction.isEmpty()) {
        prediction = matchFuzzyKey(label, excludeIndex);
    }
    if (prediction.isEmpty()) {
        prediction = matchNeighbours(label, excludeIndex);
    }

    prediction.key = label.key;
    if (prediction.isEmpty()) {
        return none;
    }

    // A rule is an explicit instruction from the user — neither adjustment
    // below is allowed to second-guess it.
    if (prediction.rung != kRungRule) {
        if (looksRecurring(label.key, signedAmount, date, excludeIndex)) {
            prediction.confidence = std::max(prediction.confidence, kRecurrenceConfidence);
            prediction.reason += QStringLiteral(" — recurs monthly at this amount");
        }
        if (!signIsPlausible(prediction.category, signedAmount)) {
            prediction.confidence = std::min(prediction.confidence, kSignMismatchCeiling);
            prediction.reason += (signedAmount > 0.0)
                ? QStringLiteral(" — but this category has only ever been an expense")
                : QStringLiteral(" — but this category has only ever been income");
        }
    }

    if (prediction.confidence < kMinConfidenceToSuggest) {
        return none; // an empty cell is cheaper for the user to fix than a wrong one
    }
    return prediction;
}

Prediction CategoryPredictor::matchRule(const NormalizedLabel& label) const
{
    for (const Data::CategoryRule& rule : m_rules) {
        if (!keyMatchesPattern(label.key, rule.pattern)) {
            continue;
        }
        Prediction prediction;
        prediction.category = rule.category;
        prediction.subcategory = rule.subcategory;
        prediction.confidence = 1.0;
        prediction.rung = kRungRule;
        prediction.reason = QStringLiteral("Your rule: labels containing \"%1\" are %2")
                                 .arg(rule.pattern, labelText(rule.category, rule.subcategory));
        return prediction;
    }
    return {};
}

CategoryPredictor::LabelChoice CategoryPredictor::chooseLabel(const QVector<int>& indices, int excludeIndex) const
{
    LabelChoice choice;

    QHash<QString, Tally> pairCounts;
    QHash<QString, Tally> categoryCounts;
    for (const int index : indices) {
        if (index == excludeIndex) {
            continue;
        }
        const Example& example = m_examples.at(index);
        addVote(pairCounts, joinPair(example.category, example.subcategory), 1.0, index);
        addVote(categoryCounts, example.category, 1.0, index);
        ++choice.total;
    }
    if (choice.total == 0) {
        return choice;
    }

    double pairWeight = 0.0;
    const QString bestPair = bestLabel(pairCounts, pairWeight);
    double categoryWeight = 0.0;
    const QString bestCategory = bestLabel(categoryCounts, categoryWeight);
    const auto bestPairCount = static_cast<int>(pairWeight);
    const auto bestCategoryCount = static_cast<int>(categoryWeight);

    const auto total = static_cast<double>(choice.total);
    const double pairPurity = static_cast<double>(bestPairCount) / total;
    const double categoryPurity = static_cast<double>(bestCategoryCount) / total;

    // The user is consistent about the category but not the subcategory (a
    // real pattern: "Groceries" every time, subcategory picked by mood).
    // Suggesting the category alone is right; guessing the subcategory would
    // be a coin flip the user has to notice and undo.
    if (categoryPurity > pairPurity + kPairAmbiguityMargin) {
        choice.category = bestCategory;
        choice.purity = categoryPurity;
        choice.support = bestCategoryCount;
        choice.droppedSubcategory = true;
        return choice;
    }

    splitPair(bestPair, choice.category, choice.subcategory);
    choice.purity = pairPurity;
    choice.support = bestPairCount;
    return choice;
}

Prediction CategoryPredictor::matchExactKey(const NormalizedLabel& label, int excludeIndex) const
{
    const auto entry = m_keyIndex.constFind(label.key);
    if (entry == m_keyIndex.constEnd()) {
        return {};
    }

    const LabelChoice choice = chooseLabel(entry.value(), excludeIndex);
    if (choice.category.isEmpty()) {
        return {};
    }

    Prediction prediction;
    prediction.category = choice.category;
    prediction.subcategory = choice.subcategory;
    prediction.rung = kRungExact;
    prediction.confidence = choice.purity * maturity(choice.total);
    prediction.reason = (choice.total == 1)
        ? QStringLiteral("Categorized \"%1\" as %2 once before")
              .arg(label.key, labelText(choice.category, choice.subcategory))
        : QStringLiteral("Categorized \"%1\" as %2 in %3 of %4 previous transactions")
              .arg(label.key, labelText(choice.category, choice.subcategory))
              .arg(choice.support)
              .arg(choice.total);
    if (choice.droppedSubcategory) {
        prediction.reason += QStringLiteral(" (subcategory varies — left blank)");
    }
    return prediction;
}

Prediction CategoryPredictor::matchFuzzyKey(const NormalizedLabel& label, int excludeIndex) const
{
    QString bestKey;
    double bestSimilarity = 0.0;
    int bestLatest = -1;

    for (auto known = m_keyIndex.constBegin(); known != m_keyIndex.constEnd(); ++known) {
        if (known.key() == label.key) {
            continue; // the exact rung already had its chance at this one
        }
        // The most recent example of the key (the indices are in increasing
        // order); a key with only the hidden example has nothing to give.
        const QVector<int>& indices = known.value();
        int latest = indices.last();
        if (latest == excludeIndex) {
            latest = indices.size() > 1 ? indices.at(indices.size() - 2) : -1;
        }
        if (latest < 0) {
            continue;
        }
        // The stored key IS its tokens joined by single spaces, so splitting
        // recovers them exactly — no second index needed.
        const double similarity =
            LabelNormalizer::tokenJaccard(label.tokens, known.key().split(QLatin1Char(' '), Qt::SkipEmptyParts));
        // At equal similarity, the key of the most recent example (#51).
        const qint64 rank = rankOf(similarity);
        const qint64 bestRank = rankOf(bestSimilarity);
        if (rank > bestRank || (rank == bestRank && rank > 0 && latest > bestLatest)) {
            bestSimilarity = similarity;
            bestKey = known.key();
            bestLatest = latest;
        }
    }

    if (bestKey.isEmpty() || bestSimilarity < kFuzzyMinSimilarity) {
        return {};
    }

    const LabelChoice choice = chooseLabel(m_keyIndex.value(bestKey), excludeIndex);
    if (choice.category.isEmpty()) {
        return {};
    }

    Prediction prediction;
    prediction.category = choice.category;
    prediction.subcategory = choice.subcategory;
    prediction.rung = kRungFuzzy;
    // Scaled by similarity: a 0.6-similar key is a weaker witness than an
    // identical one, and the confidence the user sees should say so.
    prediction.confidence = choice.purity * maturity(choice.total) * bestSimilarity;
    prediction.reason = QStringLiteral("Close to \"%1\" (%2% match), which you categorize as %3")
                             .arg(bestKey)
                             .arg(std::lround(bestSimilarity * 100.0))
                             .arg(labelText(choice.category, choice.subcategory));
    return prediction;
}

Prediction CategoryPredictor::matchNeighbours(const NormalizedLabel& label, int excludeIndex) const
{
    if (m_examples.isEmpty()) {
        return {};
    }

    // ── Query vector ──────────────────────────────────────────────────────
    // The features in a fixed order, so that each sum below is the same in
    // every run (#51).
    const QHash<QString, double> features = featuresFor(label);
    const QStringList featureOrder = sortedKeys(features);
    QHash<QString, double> queryWeights;
    double querySumOfSquares = 0.0;
    for (const QString& feature : featureOrder) {
        const double weight = features.value(feature) * inverseDocumentFrequency(feature);
        queryWeights.insert(feature, weight);
        querySumOfSquares += weight * weight;
    }
    const double queryNorm = std::sqrt(querySumOfSquares);
    if (queryNorm <= 0.0) {
        return {};
    }

    // ── Accumulate dot products, visiting only examples that share a feature ──
    QHash<int, double> dotProducts;
    for (const QString& feature : featureOrder) {
        const auto postings = m_postings.constFind(feature);
        if (postings == m_postings.constEnd()) {
            continue;
        }
        const double idf = inverseDocumentFrequency(feature);
        const double queryWeight = queryWeights.value(feature);
        for (const int index : postings.value()) {
            if (index == excludeIndex) {
                continue;
            }
            dotProducts[index] += queryWeight * m_vectors.at(index).value(feature) * idf;
        }
    }

    std::vector<std::pair<double, int>> neighbours;
    neighbours.reserve(static_cast<std::size_t>(dotProducts.size()));
    for (auto it = dotProducts.constBegin(); it != dotProducts.constEnd(); ++it) {
        const double norm = m_norms.at(it.key());
        if (norm <= 0.0) {
            continue;
        }
        const double similarity = it.value() / (queryNorm * norm);
        if (similarity >= kNeighbourMinSimilarity) {
            neighbours.emplace_back(similarity, it.key());
        }
    }
    if (neighbours.empty()) {
        return {};
    }

    // The closest first; at equal similarity (rankOf()), the most recent
    // example first (#51). A total order: the k kept are the same in every run.
    const auto keep = std::min(static_cast<std::size_t>(kNeighbourCount), neighbours.size());
    std::partial_sort(neighbours.begin(), neighbours.begin() + static_cast<std::ptrdiff_t>(keep), neighbours.end(),
                       [](const std::pair<double, int>& lhs, const std::pair<double, int>& rhs) {
                           const qint64 lhsRank = rankOf(lhs.first);
                           const qint64 rhsRank = rankOf(rhs.first);
                           return lhsRank != rhsRank ? lhsRank > rhsRank : lhs.second > rhs.second;
                       });
    neighbours.resize(keep);

    // ── Vote, weighted by similarity ──────────────────────────────────────
    QHash<QString, Tally> pairScores;
    QHash<QString, Tally> categoryScores;
    double totalScore = 0.0;
    for (const auto& [similarity, index] : neighbours) {
        const Example& example = m_examples.at(index);
        addVote(pairScores, joinPair(example.category, example.subcategory), similarity, index);
        addVote(categoryScores, example.category, similarity, index);
        totalScore += similarity;
    }
    if (totalScore <= 0.0) {
        return {};
    }

    double bestPairScore = 0.0;
    const QString bestPair = bestLabel(pairScores, bestPairScore);
    double bestCategoryScore = 0.0;
    const QString bestCategory = bestLabel(categoryScores, bestCategoryScore);

    const double pairShare = bestPairScore / totalScore;
    const double categoryShare = bestCategoryScore / totalScore;
    const double bestSimilarity = neighbours.front().first;

    Prediction prediction;
    double share = pairShare;
    bool droppedSubcategory = false;
    if (categoryShare > pairShare + kPairAmbiguityMargin) {
        prediction.category = bestCategory;
        share = categoryShare;
        droppedSubcategory = true;
    } else {
        splitPair(bestPair, prediction.category, prediction.subcategory);
    }
    if (prediction.category.isEmpty()) {
        return {};
    }

    prediction.rung = kRungSimilar;
    // Two independent discounts, both deserved: how dominant the winning
    // label was among the neighbours (`share`), and how close the closest
    // neighbour actually is. The closeness term is scaled against
    // kSimilarityFullCredit rather than used raw — raw TF-IDF cosine over
    // short labels is small even for genuine matches, so multiplying by it
    // directly would push every answer from this rung below
    // kMinConfidenceToSuggest and silently disable it. Capped so a
    // generalization never outranks two confirmed sightings of an exact key.
    const double closeness = std::min(1.0, bestSimilarity / kSimilarityFullCredit);
    prediction.confidence = std::min(kSimilarConfidenceCeiling, share * closeness);
    prediction.reason = QStringLiteral("%1 similar label(s) you categorized as %2 (closest %3% match)")
                             .arg(neighbours.size())
                             .arg(labelText(prediction.category, prediction.subcategory))
                             .arg(std::lround(bestSimilarity * 100.0));
    if (droppedSubcategory) {
        prediction.reason += QStringLiteral(" (subcategory varies — left blank)");
    }
    return prediction;
}

// ── Cross-cutting adjustments ─────────────────────────────────────────────────

bool CategoryPredictor::signIsPlausible(const QString& category, double signedAmount) const
{
    if (category.isEmpty() || signedAmount == 0.0) {
        return true; // no signal either way
    }
    const auto entry = m_categorySigns.constFind(category);
    if (entry == m_categorySigns.constEnd()) {
        return true; // never seen this category — nothing to contradict
    }

    const int credits = entry.value().first;
    const int debits = entry.value().second;
    if (signedAmount > 0.0) {
        // Incoming money. Fine if this category has ever been income, or if
        // we simply haven't seen it enough times to claim it never is.
        return credits > 0 || debits < kSignGateMinExamples;
    }
    return debits > 0 || credits < kSignGateMinExamples;
}

bool CategoryPredictor::looksRecurring(const QString& key, double signedAmount,
                                         const QDate& date, int excludeIndex) const
{
    if (key.isEmpty() || !date.isValid()) {
        return false;
    }
    const auto entry = m_keyIndex.constFind(key);
    if (entry == m_keyIndex.constEnd()) {
        return false;
    }

    // Absolute floor of one cent so a zero-amount row can't match everything.
    const double tolerance = std::max(0.01, std::abs(signedAmount) * kRecurrenceTolerance);

    QVector<QDate> dates;
    for (const int index : entry.value()) {
        if (index == excludeIndex) {
            continue;
        }
        const Example& example = m_examples.at(index);
        if (!example.date.isValid()) {
            continue;
        }
        if (std::abs(example.signedAmount - signedAmount) > tolerance) {
            continue;
        }
        dates.append(example.date);
    }
    if (dates.size() < 2) {
        return false; // one prior sighting is a coincidence, not a pattern
    }
    std::sort(dates.begin(), dates.end());

    const auto inWindow = [](qint64 days) {
        return days >= kRecurrenceMinGapDays && days <= kRecurrenceMaxGapDays;
    };

    // The new row must land about a month after the most recent sighting...
    if (!inWindow(dates.last().daysTo(date))) {
        return false;
    }
    // ...and the prior sightings must themselves be about a month apart, so a
    // pair of coincidentally-equal amounts doesn't read as a subscription.
    for (qsizetype i = 1; i < dates.size(); ++i) {
        if (inWindow(dates.at(i - 1).daysTo(dates.at(i)))) {
            return true;
        }
    }
    return false;
}

// ── Evaluation ────────────────────────────────────────────────────────────────

Evaluation CategoryPredictor::evaluate() const
{
    Evaluation evaluation;

    const int count = static_cast<int>(m_examples.size());
    double confidenceSum = 0.0;

    for (int i = 0; i < count; ++i) {
        const Example& example = m_examples.at(i);
        if (example.key.isEmpty() || example.category.isEmpty()) {
            continue;
        }
        ++evaluation.total;

        NormalizedLabel label;
        label.key = example.key;
        label.tokens = example.tokens;

        // useRules = false: a rule the user created *from* this very row
        // would score itself, which would make the number meaningless.
        const Prediction prediction = predictFor(label, example.signedAmount, example.date, i, false);
        if (prediction.isEmpty()) {
            continue;
        }

        ++evaluation.attempted;
        confidenceSum += prediction.confidence;
        ++evaluation.attemptsByRung[prediction.rung];

        if (prediction.category == example.category) {
            ++evaluation.categoryCorrect;
            ++evaluation.correctByRung[prediction.rung];
            if (prediction.subcategory == example.subcategory) {
                ++evaluation.pairCorrect;
            }
        }
    }

    if (evaluation.attempted > 0) {
        evaluation.meanConfidence = confidenceSum / static_cast<double>(evaluation.attempted);
    }
    return evaluation;
}

// ── Conversions from the persisted row shape ──────────────────────────────────

Example CategoryPredictor::exampleFrom(const Data::Transaction& transaction)
{
    const NormalizedLabel label = LabelNormalizer::normalize(transaction.name, transaction.description);

    Example example;
    example.key = label.key;
    example.tokens = label.tokens;
    example.category = transaction.category;
    example.subcategory = transaction.subcategory;
    example.signedAmount = transaction.credit - transaction.debit;
    // operationDate is when the money actually moved, which is what a monthly
    // recurrence is spaced by; confirmationDate is the bank's booking date and
    // only used when the better one is missing.
    example.date = transaction.operationDate.isValid() ? transaction.operationDate
                                                        : transaction.confirmationDate;
    return example;
}

QVector<Example> CategoryPredictor::examplesFromTransactions(const QVector<Data::Transaction>& transactions)
{
    QVector<Example> examples;
    examples.reserve(transactions.size());
    for (const Data::Transaction& transaction : transactions) {
        if (transaction.category.isEmpty()) {
            continue; // an uncategorized row is not a training example
        }
        examples.append(exampleFrom(transaction));
    }
    return examples;
}

} // namespace Predict
