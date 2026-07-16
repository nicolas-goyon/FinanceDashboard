#include "CashFlowController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QDateTime>

namespace {

struct Category { const char* name; const char* color; };

const Category kCats[6] = {
    { "Rent",      "#f38ba8" },
    { "Groceries", "#a6e3a1" },
    { "Transport", "#89b4fa" },
    { "Utilities", "#fab387" },
    { "Leisure",   "#f9e2af" },
    { "Other",     "#cba6f7" },
};

} // namespace

CashFlowController::CashFlowController(QObject* parent)
    : QObject(parent)
{
    // MOCK: category spending per month
    const double data[kCategories][kMonths] = {
        // Rent (fixed)
        { 950, 950, 950, 950, 950, 950, 950, 950, 950, 950, 950, 950 },
        // Groceries
        { 410, 395, 430, 385, 445, 420, 405, 390, 450, 425, 415, 440 },
        // Transport
        { 145, 130, 160, 125, 170, 150, 135, 155, 180, 140, 165, 150 },
        // Utilities
        { 175, 180, 160, 150, 140, 135, 130, 145, 155, 170, 185, 190 },
        // Leisure
        { 260, 310, 205, 340, 280, 225, 350, 240, 300, 265, 330, 290 },
        // Other
        { 180, 220, 160, 290, 175, 240, 195, 300, 210, 185, 260, 230 },
    };
    for (int c = 0; c < kCategories; ++c)
        for (int m = 0; m < kMonths; ++m)
            m_categoryData[c][m] = data[c][m];

    // MOCK: monthly income
    const double income[kMonths] = { 3250, 3250, 3250, 3250, 3250, 3380,
                                     3380, 3380, 3380, 3380, 3380, 3380 };

    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-11);

    QVector<double> expenses(kMonths, 0.0);
    for (int m = 0; m < kMonths; ++m) {
        for (int c = 0; c < kCategories; ++c)
            expenses[m] += data[c][m];

        const QDate month = firstMonth.addMonths(m);
        m_monthLabels.append(month.toString("MMM"));
        m_timestamps.append(double(month.startOfDay().toMSecsSinceEpoch()));
        m_incomeValues.append(income[m]);
        m_expenseValues.append(expenses[m]);
        m_barsMax = qMax(m_barsMax, qMax(income[m], expenses[m]));
    }
    m_barsMax *= 1.15;
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    // Rolling 3-month average of expenses
    for (int m = 0; m < kMonths; ++m) {
        double sum = 0; int n = 0;
        for (int k = qMax(0, m - 2); k <= m; ++k) { sum += expenses[k]; ++n; }
        m_rollingValues.append(sum / n);
    }

    for (const Category& c : kCats)
        m_categories.append(QVariantMap{ { "name", c.name }, { "color", c.color } });
}

void CashFlowController::setSelectedMonth(int month)
{
    if (month < 0 || month >= kMonths || month == m_selectedMonth) return;
    m_selectedMonth = month;
    emit monthChanged();
}

QVariantList CashFlowController::monthSlices() const
{
    QVariantList slices;
    for (int c = 0; c < kCategories; ++c)
        slices.append(QVariantMap{
            { "label", kCats[c].name },
            { "value", m_categoryData[c][m_selectedMonth] },
            { "color", kCats[c].color },
        });
    return slices;
}

QVariantList CashFlowController::biggestRows() const
{
    // MOCK: a plausible "biggest expenses" list derived from the category data
    const int m = m_selectedMonth;
    QVariantList rows;
    auto add = [&](const QString& label, int cat, double amount) {
        rows.append(QVariantMap{
            { "label", label },
            { "category", kCats[cat].name },
            { "amount", Fmt::money(amount) },
            { "color", kCats[cat].color },
        });
    };
    add("Monthly rent",            0, m_categoryData[0][m]);
    add("Supermarket — largest",   1, m_categoryData[1][m] * 0.45);
    add("Leisure — largest",       4, m_categoryData[4][m] * 0.55);
    add("Misc — largest",          5, m_categoryData[5][m] * 0.60);
    add("Electricity bill",        3, m_categoryData[3][m] * 0.65);
    return rows;
}

QVariantList CashFlowController::flowInputs() const
{
    // MOCK: one salary input; totals match flowOutputs()
    double income = m_incomeValues[m_selectedMonth].toDouble();
    return {
        QVariantMap{ { "name", "Salary" }, { "value", income }, { "color", "#a6e3a1" } },
    };
}

QVariantList CashFlowController::flowOutputs() const
{
    QVariantList out;
    const int m = m_selectedMonth;
    double spent = 0;
    for (int c = 0; c < kCategories; ++c) {
        const double v = m_categoryData[c][m];
        spent += v;
        out.append(QVariantMap{
            { "name", kCats[c].name },
            { "value", v },
            { "color", kCats[c].color },
        });
    }
    // Leftover = savings — keeps input and output totals equal.
    const double leftover = m_incomeValues[m].toDouble() - spent;
    out.append(QVariantMap{
        { "name", "Savings" },
        { "value", qMax(leftover, 0.0) },
        { "color", "#94e2d5" },
    });
    return out;
}

QVariantList CashFlowController::categoryValues(int index) const
{
    QVariantList v;
    if (index < 0 || index >= kCategories) return v;
    for (int m = 0; m < kMonths; ++m)
        v.append(m_categoryData[index][m]);
    return v;
}
