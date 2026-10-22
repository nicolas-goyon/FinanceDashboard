// Standalone sanity harness for Predict::LabelNormalizer / CategoryPredictor.
// Not part of the app build (src/predict/ is compiled into FinanceDashboard,
// this file is not). CTest runs it as the test `predictor_selftest`
// (tests/CMakeLists.txt, with -DFD_BUILD_TESTS=ON), and so does the CI.
//
// Prints one line per check and exits non-zero if any failed. Deliberately
// dependency-free (no Qt Test) so it runs anywhere Qt6Core is available.

#include "predict/CategoryPredictor.h"
#include "predict/LabelNormalizer.h"

#include <QDate>
#include <QHashFunctions>
#include <QString>
#include <QVector>

#include <cstdio>

namespace {

int gFailures = 0;

void check(bool condition, const QString& what)
{
    std::printf("%s  %s\n", condition ? "PASS" : "FAIL", qPrintable(what));
    if (!condition) {
        ++gFailures;
    }
}

void checkEqual(const QString& actual, const QString& expected, const QString& what)
{
    const bool ok = (actual == expected);
    std::printf("%s  %s  (got \"%s\", expected \"%s\")\n", ok ? "PASS" : "FAIL",
                 qPrintable(what), qPrintable(actual), qPrintable(expected));
    if (!ok) {
        ++gFailures;
    }
}

Predict::Example makeExample(const QString& name, const QString& category, const QString& subcategory,
                              double signedAmount, const QDate& date)
{
    const Predict::NormalizedLabel label = Predict::LabelNormalizer::normalize(name);
    Predict::Example example;
    example.key = label.key;
    example.tokens = label.tokens;
    example.category = category;
    example.subcategory = subcategory;
    example.signedAmount = signedAmount;
    example.date = date;
    return example;
}

void testNormalizer()
{
    std::printf("\n── LabelNormalizer ─────────────────────────────────────────\n");

    checkEqual(Predict::LabelNormalizer::normalize(QStringLiteral("CARTE 12/03 CARREFOUR MARKET 4972 LYON")).key,
                QStringLiteral("CARREFOUR MARKET LYON"), QStringLiteral("card noise stripped"));

    checkEqual(Predict::LabelNormalizer::normalize(QStringLiteral("VIR SEPA CARREFOUR MARKET")).key,
                QStringLiteral("CARREFOUR MARKET"), QStringLiteral("transfer prefix stripped"));

    checkEqual(Predict::LabelNormalizer::normalize(QStringLiteral("PRLV SEPA EDF")).key,
                QStringLiteral("EDF"), QStringLiteral("direct debit prefix stripped"));

    checkEqual(Predict::LabelNormalizer::normalize(QStringLiteral("Crédit Agricole  Intérêts")).key,
                QStringLiteral("CREDIT AGRICOLE INTERETS"), QStringLiteral("accents removed, case folded"));

    // Every token is boilerplate — the fallback pass must still produce a key.
    check(!Predict::LabelNormalizer::normalize(QStringLiteral("VIREMENT EN VOTRE FAVEUR")).key.isEmpty(),
           QStringLiteral("all-boilerplate label still yields a key"));

    checkEqual(Predict::LabelNormalizer::normalize(QStringLiteral("  "), QStringLiteral("PAIEMENT CB SPOTIFY")).key,
                QStringLiteral("SPOTIFY"), QStringLiteral("falls back to the description"));

    check(Predict::LabelNormalizer::normalize(QStringLiteral("")).key.isEmpty(),
           QStringLiteral("empty input yields an empty key"));

    const double self = Predict::LabelNormalizer::tokenJaccard({ QStringLiteral("A"), QStringLiteral("B") },
                                                                { QStringLiteral("A"), QStringLiteral("B") });
    check(self > 0.999, QStringLiteral("jaccard of identical token sets is 1"));
    check(Predict::LabelNormalizer::tokenJaccard({}, {}) == 0.0,
           QStringLiteral("jaccard of two empty sets is 0, not 1"));
}

void testCascade()
{
    std::printf("\n── CategoryPredictor cascade ───────────────────────────────\n");

    QVector<Predict::Example> examples;
    examples << makeExample(QStringLiteral("CARTE 03/01 CARREFOUR MARKET 4972"), QStringLiteral("Groceries"),
                             QStringLiteral("Supermarket"), -42.10, QDate(2026, 1, 3));
    examples << makeExample(QStringLiteral("CARTE 11/02 CARREFOUR MARKET 4972"), QStringLiteral("Groceries"),
                             QStringLiteral("Supermarket"), -55.30, QDate(2026, 2, 11));
    examples << makeExample(QStringLiteral("CARTE 07/01 SNCF CONNECT"), QStringLiteral("Transport"),
                             QStringLiteral("Train"), -89.00, QDate(2026, 1, 7));
    examples << makeExample(QStringLiteral("VIR SEPA SALAIRE ACME SAS"), QStringLiteral("Income"),
                             QStringLiteral("Salary"), 2450.00, QDate(2026, 1, 28));

    Predict::CategoryPredictor predictor;
    predictor.train(examples, {});

    check(predictor.isTrained(), QStringLiteral("predictor reports trained"));
    check(predictor.exampleCount() == 4, QStringLiteral("all four examples kept"));

    // ── exact rung ────────────────────────────────────────────────────────
    const Predict::Prediction exact =
        predictor.predict(QStringLiteral("CARTE 04/03 CARREFOUR MARKET 4972"), {}, -30.0, QDate(2026, 3, 4));
    checkEqual(exact.rung, QStringLiteral("exact"), QStringLiteral("repeat merchant hits the exact rung"));
    checkEqual(exact.category, QStringLiteral("Groceries"), QStringLiteral("exact rung picks the right category"));
    check(exact.confidence > 0.79 && exact.confidence < 0.81,
           QStringLiteral("two prior sightings score 0.80"));

    // ── single-sighting calibration ───────────────────────────────────────
    const Predict::Prediction once =
        predictor.predict(QStringLiteral("CARTE 09/03 SNCF CONNECT"), {}, -14.0, QDate(2026, 3, 9));
    check(once.confidence > 0.66 && once.confidence < 0.68,
           QStringLiteral("one prior sighting scores 0.67 (amber, needs review)"));

    // ── fuzzy rung ────────────────────────────────────────────────────────
    const Predict::Prediction fuzzy =
        predictor.predict(QStringLiteral("CARTE 04/03 CARREFOUR MARKET 4972 LYON"), {}, -30.0, QDate(2026, 3, 4));
    checkEqual(fuzzy.rung, QStringLiteral("fuzzy"), QStringLiteral("near-miss label hits the fuzzy rung"));
    checkEqual(fuzzy.category, QStringLiteral("Groceries"), QStringLiteral("fuzzy rung generalizes the merchant"));
    check(fuzzy.confidence < exact.confidence,
           QStringLiteral("fuzzy is less confident than exact"));

    // ── similar rung (shares one token, not enough for jaccard ≥ 0.6) ─────
    const Predict::Prediction similar =
        predictor.predict(QStringLiteral("CARTE 12/03 SNCF GARE MONTPARNASSE PARIS"), {}, -60.0, QDate(2026, 3, 12));
    checkEqual(similar.rung, QStringLiteral("similar"), QStringLiteral("token overlap only hits the similar rung"));
    checkEqual(similar.category, QStringLiteral("Transport"), QStringLiteral("similar rung transfers the token"));
    check(similar.confidence <= Predict::kSimilarConfidenceCeiling,
           QStringLiteral("similar rung respects its confidence ceiling"));

    // ── unknown label ─────────────────────────────────────────────────────
    const Predict::Prediction unknown =
        predictor.predict(QStringLiteral("CARTE 12/03 ZZQQ WIDGET EMPORIUM"), {}, -12.0, QDate(2026, 3, 12));
    check(unknown.isEmpty(), QStringLiteral("unrelated label returns no suggestion"));
    check(!unknown.key.isEmpty(), QStringLiteral("...but still reports the merchant key it computed"));

    // ── rules ─────────────────────────────────────────────────────────────
    Data::CategoryRule rule;
    rule.id = 1;
    rule.pattern = QStringLiteral("CARREFOUR MARKET");
    rule.category = QStringLiteral("Household");
    rule.subcategory = QStringLiteral("Supplies");
    predictor.train(examples, { rule });

    const Predict::Prediction ruled =
        predictor.predict(QStringLiteral("CARTE 04/03 CARREFOUR MARKET 4972 LYON"), {}, -30.0, QDate(2026, 3, 4));
    checkEqual(ruled.rung, QStringLiteral("rule"), QStringLiteral("a rule outranks the learned memory"));
    checkEqual(ruled.category, QStringLiteral("Household"), QStringLiteral("rule sets the category"));
    check(ruled.confidence >= 1.0, QStringLiteral("rule is fully confident"));
}

void testAdjustments()
{
    std::printf("\n── Sign gating / recurrence / ambiguity ────────────────────\n");

    // ── sign gating ───────────────────────────────────────────────────────
    QVector<Predict::Example> examples;
    for (int month = 1; month <= 4; ++month) {
        examples << makeExample(QStringLiteral("CARTE MONOPRIX"), QStringLiteral("Groceries"),
                                 QStringLiteral("Supermarket"), -40.0, QDate(2026, month, 5));
    }
    Predict::CategoryPredictor predictor;
    predictor.train(examples, {});

    const Predict::Prediction refund =
        predictor.predict(QStringLiteral("CARTE MONOPRIX"), {}, 40.0, QDate(2026, 5, 5));
    check(refund.confidence <= Predict::kSignMismatchCeiling,
           QStringLiteral("credit against a debit-only category is capped"));

    // ── recurrence ────────────────────────────────────────────────────────
    QVector<Predict::Example> rent;
    rent << makeExample(QStringLiteral("VIR SEPA LOYER APPART"), QStringLiteral("Housing"),
                         QStringLiteral("Rent"), -780.0, QDate(2026, 1, 5));
    rent << makeExample(QStringLiteral("VIR SEPA LOYER APPART"), QStringLiteral("Housing"),
                         QStringLiteral("Rent"), -780.0, QDate(2026, 2, 5));
    Predict::CategoryPredictor rentPredictor;
    rentPredictor.train(rent, {});

    const Predict::Prediction recurring =
        rentPredictor.predict(QStringLiteral("VIR SEPA LOYER APPART"), {}, -780.0, QDate(2026, 3, 5));
    check(recurring.confidence >= Predict::kRecurrenceConfidence,
           QStringLiteral("monthly same-amount row is boosted to 0.95"));

    const Predict::Prediction offAmount =
        rentPredictor.predict(QStringLiteral("VIR SEPA LOYER APPART"), {}, -1200.0, QDate(2026, 3, 5));
    check(offAmount.confidence < Predict::kRecurrenceConfidence,
           QStringLiteral("different amount is not treated as the recurrence"));

    // ── subcategory ambiguity ─────────────────────────────────────────────
    QVector<Predict::Example> mixed;
    mixed << makeExample(QStringLiteral("CARTE AMAZON"), QStringLiteral("Shopping"),
                          QStringLiteral("Books"), -20.0, QDate(2026, 1, 4));
    mixed << makeExample(QStringLiteral("CARTE AMAZON"), QStringLiteral("Shopping"),
                          QStringLiteral("Electronics"), -20.0, QDate(2026, 2, 9));
    mixed << makeExample(QStringLiteral("CARTE AMAZON"), QStringLiteral("Shopping"),
                          QStringLiteral("Home"), -20.0, QDate(2026, 3, 14));
    Predict::CategoryPredictor mixedPredictor;
    mixedPredictor.train(mixed, {});

    const Predict::Prediction ambiguous =
        mixedPredictor.predict(QStringLiteral("CARTE AMAZON"), {}, -25.0, QDate(2026, 4, 20));
    checkEqual(ambiguous.category, QStringLiteral("Shopping"), QStringLiteral("consistent category is suggested"));
    check(ambiguous.subcategory.isEmpty(),
           QStringLiteral("inconsistent subcategory is left blank rather than guessed"));
}

// True when `runs` predictors, each trained with a new random seed for its
// QHash members, all suggest `expected` from `rung` for `name` (#51). `seen`
// gets the first other answer.
bool alwaysSuggests(const QVector<Predict::Example>& examples, const QString& name, const QString& expected,
                    const QString& rung, QString& seen)
{
    constexpr int kRuns = 25;
    for (int run = 0; run < kRuns; ++run) {
        QHashSeed::resetRandomGlobalSeed();
        Predict::CategoryPredictor predictor;
        predictor.train(examples, {});
        const Predict::Prediction prediction = predictor.predict(name, {}, -20.0, QDate(2026, 9, 1));
        if (prediction.category != expected || prediction.rung != rung) {
            seen = prediction.category + QStringLiteral(" (") + prediction.rung + QStringLiteral(")");
            return false;
        }
    }
    seen = expected + QStringLiteral(" (") + rung + QStringLiteral(")");
    return true;
}

void testTies()
{
    std::printf("\n── Ties: the most recent example wins, in every run (#51) ──\n");
    QString seen;
    bool ok = false;

    // ── exact rung: as many Transport as Leisure for one key ──────────────
    // The input order is not the date order: train() sorts by date.
    QVector<Predict::Example> exact;
    for (int month = 1; month <= 3; ++month) {
        exact << makeExample(QStringLiteral("CARTE SNCF CONNECT"), QStringLiteral("Leisure"), QStringLiteral("Trips"),
                             -20.0, QDate(2026, month, 10));
        exact << makeExample(QStringLiteral("CARTE SNCF CONNECT"), QStringLiteral("Transport"), QStringLiteral("Train"),
                             -20.0, QDate(2026, month, 5));
    }
    ok = alwaysSuggests(exact, QStringLiteral("CARTE SNCF CONNECT"), QStringLiteral("Leisure"), QStringLiteral("exact"), seen);
    check(ok, QStringLiteral("exact rung, 3 against 3: the label of the most recent row (%1)").arg(seen));
    exact.prepend(makeExample(QStringLiteral("CARTE SNCF CONNECT"), QStringLiteral("Transport"), QStringLiteral("Train"),
                              -20.0, QDate(2026, 4, 5)));
    exact.append(makeExample(QStringLiteral("CARTE SNCF CONNECT"), QStringLiteral("Leisure"), QStringLiteral("Trips"),
                             -20.0, QDate(2026, 3, 20)));
    ok = alwaysSuggests(exact, QStringLiteral("CARTE SNCF CONNECT"), QStringLiteral("Transport"), QStringLiteral("exact"), seen);
    check(ok, QStringLiteral("exact rung, 4 against 4: the most recent row is now Transport (%1)").arg(seen));

    // ── fuzzy rung: two known keys equally close to the label ─────────────
    QVector<Predict::Example> fuzzy;
    fuzzy << makeExample(QStringLiteral("CARTE CARREFOUR CITY"), QStringLiteral("Household"), QString(), -20.0,
                         QDate(2026, 2, 3));
    fuzzy << makeExample(QStringLiteral("CARTE CARREFOUR MARKET"), QStringLiteral("Groceries"), QString(), -20.0,
                         QDate(2026, 1, 3));
    ok = alwaysSuggests(fuzzy, QStringLiteral("CARTE CARREFOUR MARKET CITY"), QStringLiteral("Household"),
                         QStringLiteral("fuzzy"), seen);
    check(ok, QStringLiteral("fuzzy rung, equal similarity: the key of the most recent row (%1)").arg(seen));
    fuzzy << makeExample(QStringLiteral("CARTE CARREFOUR MARKET"), QStringLiteral("Groceries"), QString(), -20.0,
                         QDate(2026, 3, 3));
    ok = alwaysSuggests(fuzzy, QStringLiteral("CARTE CARREFOUR MARKET CITY"), QStringLiteral("Groceries"),
                         QStringLiteral("fuzzy"), seen);
    check(ok, QStringLiteral("fuzzy rung: a newer row moves the tie to the other key (%1)").arg(seen));

    // ── similar rung: six equally close neighbours, five are kept ─────────
    // Each label shares only "SNCF" with the query, so the six are equally
    // similar; the five most recent vote.
    const QStringList words = {QStringLiteral("ALPHA"), QStringLiteral("BRAVO"), QStringLiteral("DELTA"),
                               QStringLiteral("TANGO"), QStringLiteral("OSCAR"), QStringLiteral("HOTEL")};
    const auto neighbours = [&words](const QString& oldest, const QString& other) {
        QVector<Predict::Example> examples;
        for (int month = 1; month <= 6; ++month) {
            examples << makeExample(QStringLiteral("CARTE SNCF ") + words.at(month - 1),
                                    (month % 2 == 1) ? oldest : other, QString(), -20.0, QDate(2026, month, 1));
        }
        return examples;
    };
    // January Transport, then Leisure, Transport, ...: the kept five are
    // three Leisure and two Transport.
    ok = alwaysSuggests(neighbours(QStringLiteral("Transport"), QStringLiteral("Leisure")), QStringLiteral("CARTE SNCF"),
                         QStringLiteral("Leisure"), QStringLiteral("similar"), seen);
    check(ok, QStringLiteral("similar rung, six equal neighbours: the five most recent vote (%1)").arg(seen));
    ok = alwaysSuggests(neighbours(QStringLiteral("Leisure"), QStringLiteral("Transport")), QStringLiteral("CARTE SNCF"),
                         QStringLiteral("Transport"), QStringLiteral("similar"), seen);
    check(ok, QStringLiteral("similar rung, the other way round (%1)").arg(seen));

    // ── rules: two rules match, same priority and same pattern length ────
    // The rule made last (the higher id) applies, not the first pattern in
    // alphabetical order.
    const auto ruleFor = [](int id, const QString& pattern, const QString& category) {
        Data::CategoryRule rule;
        rule.id = id;
        rule.pattern = pattern;
        rule.category = category;
        return rule;
    };
    const auto ruleCategory = [](const QVector<Data::CategoryRule>& rules) {
        Predict::CategoryPredictor predictor;
        predictor.train({}, rules);
        return predictor.predict(QStringLiteral("AUCHAN CASINO"), {}, -20.0, QDate(2026, 9, 1)).category;
    };
    const Data::CategoryRule auchanFirst = ruleFor(1, QStringLiteral("AUCHAN"), QStringLiteral("Groceries"));
    const Data::CategoryRule casinoLast = ruleFor(2, QStringLiteral("CASINO"), QStringLiteral("Household"));
    checkEqual(ruleCategory({auchanFirst, casinoLast}), QStringLiteral("Household"),
               QStringLiteral("two rules of equal rank match: the rule made last applies"));
    checkEqual(ruleCategory({casinoLast, auchanFirst}), QStringLiteral("Household"),
               QStringLiteral("...in every input order"));
    const Data::CategoryRule casinoFirst = ruleFor(1, QStringLiteral("CASINO"), QStringLiteral("Household"));
    const Data::CategoryRule auchanLast = ruleFor(2, QStringLiteral("AUCHAN"), QStringLiteral("Groceries"));
    checkEqual(ruleCategory({casinoFirst, auchanLast}), QStringLiteral("Groceries"),
               QStringLiteral("...and the other way round"));
}

void testEvaluation()
{
    std::printf("\n── Leave-one-out evaluation ────────────────────────────────\n");

    QVector<Predict::Example> examples;
    for (int month = 1; month <= 6; ++month) {
        examples << makeExample(QStringLiteral("CARTE MONOPRIX"), QStringLiteral("Groceries"),
                                 QStringLiteral("Supermarket"), -40.0 - month, QDate(2026, month, 5));
        examples << makeExample(QStringLiteral("VIR SEPA SALAIRE ACME"), QStringLiteral("Income"),
                                 QStringLiteral("Salary"), 2400.0, QDate(2026, month, 28));
    }

    Predict::CategoryPredictor predictor;
    predictor.train(examples, {});
    const Predict::Evaluation evaluation = predictor.evaluate();

    std::printf("      total=%d attempted=%d categoryCorrect=%d pairCorrect=%d meanConfidence=%.2f\n",
                 evaluation.total, evaluation.attempted, evaluation.categoryCorrect,
                 evaluation.pairCorrect, evaluation.meanConfidence);

    check(evaluation.total == 12, QStringLiteral("every labeled example is evaluated"));
    check(evaluation.attempted == evaluation.total,
           QStringLiteral("every example gets a suggestion from the rest"));
    check(evaluation.categoryCorrect == evaluation.total,
           QStringLiteral("leave-one-out accuracy is 100% on a clean set"));
    check(!evaluation.attemptsByRung.isEmpty(), QStringLiteral("per-rung breakdown is populated"));
}

} // namespace

int main()
{
    testNormalizer();
    testCascade();
    testAdjustments();
    testTies();
    testEvaluation();

    std::printf("\n%s — %d failure(s)\n", gFailures == 0 ? "ALL CHECKS PASSED" : "FAILURES", gFailures);
    return gFailures == 0 ? 0 : 1;
}
