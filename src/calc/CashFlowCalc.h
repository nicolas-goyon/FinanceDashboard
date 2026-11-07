#ifndef FINANCEDASHBOARD_CALC_CASHFLOWCALC_H
#define FINANCEDASHBOARD_CALC_CASHFLOWCALC_H

#include "models/Transaction.h"

#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

// ── CashFlowCalc ─────────────────────────────────────────────────────────────
//
// The figures of the Cash Flow page (#210), with no database: the income and
// the spending of the last 12 months, by category, from the rows of the
// accounts of the net worth. The internal transfers (#207) are left out: a
// transfer to the Livret A is not spending. Whole cents (#33).
//
// Income = the credits, spending = the debits. A row with no category is in
// "Uncategorized".
namespace CashFlowCalc {

inline constexpr int kMonths = 12;
// The stacked bars: the 5 categories with the most spending over the 12
// months, then "Other" for the rest. The QML declares one BarSet for each.
inline constexpr int kStackedCategories = 6;
inline constexpr int kBiggestRows = 5;
inline constexpr int kRollingWindow = 3;

[[nodiscard]] QString uncategorized();
[[nodiscard]] QString other();

struct Result {
    QVector<QDate> months;                       // the first day of each month, oldest first
    QVector<qint64> incomeCents;                 // [month]
    QVector<qint64> spendingCents;               // [month]
    QVector<QHash<QString, qint64>> byCategory;  // [month] category -> spending
    QVector<double> rolling;                     // the 3-month average, from the third month
    QStringList stackedNames;                    // kStackedCategories names (some can be empty)
    QVector<QVector<qint64>> stackedCents;       // [stacked category][month]
    QVector<QVector<Data::Transaction>> biggest; // [month] the kBiggestRows largest debits
};

// The 12 months that end with the month of `today`.
[[nodiscard]] Result compute(const QVector<Data::Transaction>& rows, const QDate& today);

// One month of the money flow: [{ name, value, color }] for each side, with
// equal totals. What is left of the income goes out as "Savings". Spending
// above the income comes in as "Overspend" (taken from the savings), so the
// outputs do not add up to more than the inputs (#42). A part with no amount
// is left out.
struct Part {
    QString name;
    double value = 0.0;
    QString color;
};
struct FlowSides {
    QVariantList inputs;
    QVariantList outputs;
};
[[nodiscard]] FlowSides moneyFlow(double income, const QVector<Part>& spending);

} // namespace CashFlowCalc

#endif // FINANCEDASHBOARD_CALC_CASHFLOWCALC_H
