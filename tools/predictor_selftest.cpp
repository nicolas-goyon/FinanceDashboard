// Standalone sanity harness for Predict::LabelNormalizer / CategoryPredictor.
// Not part of the app build (see CMakeLists.txt — src/predict/ is compiled
// into FinanceDashboard, this file is not). Build and run it by hand when
// touching the predictor:
//
//   g++ -std=c++17 -fPIC -I src -I <qt6-include-dirs>
//       tools/predictor_selftest.cpp src/predict/LabelNormalizer.cpp
//       src/predict/CategoryPredictor.cpp -lQt6Core -o /tmp/selftest
//   /tmp/selftest
//
// (one command — split across lines here without a trailing backslash, which
// would open a multi-line comment and trip -Wcomment)
//
// Prints one line per check and exits non-zero if any failed. Deliberately
// dependency-free (no Qt Test) so it runs anywhere Qt6Core is available —
// see TODO.md's "no tests" item for the proper Qt Test suite this stands in
// for.

#include "predict/CategoryPredictor.h"
#include "predict/LabelNormalizer.h"

#include <QDate>
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
    testEvaluation();

    std::printf("\n%s — %d failure(s)\n", gFailures == 0 ? "ALL CHECKS PASSED" : "FAILURES", gFailures);
    return gFailures == 0 ? 0 : 1;
}
