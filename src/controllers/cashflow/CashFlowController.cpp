#include "CashFlowController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QVector>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)
#include <QtMinMax>

#include <array>

namespace {

struct Category { const char* name; const char* color; };

const std::array<Category, 6> kCats = {{
    { "Rent",      "#f38ba8" },
    { "Groceries", "#a6e3a1" },
    { "Transport", "#89b4fa" },
    { "Utilities", "#fab387" },
    { "Leisure",   "#f9e2af" },
    { "Other",     "#cba6f7" },
}};

// Indices into kCats / m_categoryData — named so biggestRows() below doesn't
// pass raw magic numbers (cppcoreguidelines-avoid-magic-numbers).
constexpr int kCatRent      = 0;
constexpr int kCatGroceries = 1;
constexpr int kCatUtilities = 3;
constexpr int kCatLeisure   = 4;
constexpr int kCatOther     = 5;

constexpr double kBarsAxisHeadroom = 1.15; // 15% headroom above the max bar value for the chart axis

// MOCK: fractions applied to the selected month's category totals to derive
// a plausible "biggest expenses" list (biggestRows()).
constexpr double kGroceriesBiggestFactor = 0.45;
constexpr double kLeisureBiggestFactor   = 0.55;
constexpr double kOtherBiggestFactor     = 0.60;
constexpr double kUtilitiesBiggestFactor = 0.65;

} // namespace

CashFlowController::CashFlowController(QObject* parent)
    : QObject(parent)
{
    // MOCK: category spending per month
    const std::array<std::array<double, kMonths>, kCategories> data = {{
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
    }};
    for (int catIdx = 0; catIdx < kCategories; ++catIdx) {
        for (int monthIdx = 0; monthIdx < kMonths; ++monthIdx) {
            m_categoryData.at(catIdx).at(monthIdx) = data.at(catIdx).at(monthIdx);
        }
    }

    // MOCK: monthly income
    const std::array<double, kMonths> income = { 3250, 3250, 3250, 3250, 3250, 3380,
                                                 3380, 3380, 3380, 3380, 3380, 3380 };

    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-11);

    // Built with append() (monthIdx runs 0..kMonths-1 in order) rather than a
    // pre-sized QVector written by index — QVector/QList's operator[] has no
    // bounds-checked mutable equivalent (at() is const-only), so appending
    // avoids indexed writes entirely instead of suppressing the warning.
    QVector<double> expenses;
    expenses.reserve(kMonths);
    for (int monthIdx = 0; monthIdx < kMonths; ++monthIdx) {
        double monthTotal = 0.0;
        for (const auto& row : data) {
            monthTotal += row.at(monthIdx);
        }
        expenses.append(monthTotal);

        const QDate month = firstMonth.addMonths(monthIdx);
        m_monthLabels.append(month.toString("MMM"));
        m_timestamps.append(static_cast<double>(month.startOfDay().toMSecsSinceEpoch()));
        m_incomeValues.append(income.at(monthIdx));
        m_expenseValues.append(monthTotal);
        m_barsMax = qMax(m_barsMax, qMax(income.at(monthIdx), monthTotal));
    }
    m_barsMax *= kBarsAxisHeadroom;
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    // Rolling 3-month average of expenses
    for (int monthIdx = 0; monthIdx < kMonths; ++monthIdx) {
        double sum = 0; int count = 0;
        for (int k = qMax(0, monthIdx - 2); k <= monthIdx; ++k) { sum += expenses.at(k); ++count; }
        m_rollingValues.append(sum / count);
    }

    for (const Category& category : kCats) {
        m_categories.append(QVariantMap{ { "name", category.name }, { "color", category.color } });
    }
}

void CashFlowController::setSelectedMonth(int month)
{
    if (month < 0 || month >= kMonths || month == m_selectedMonth) { return; }
    m_selectedMonth = month;
    emit monthChanged();
}

QVariantList CashFlowController::monthSlices() const
{
    QVariantList slices;
    for (int catIdx = 0; catIdx < kCategories; ++catIdx) {
        slices.append(QVariantMap{
            { "label", kCats.at(catIdx).name },
            { "value", m_categoryData.at(catIdx).at(m_selectedMonth) },
            { "color", kCats.at(catIdx).color },
        });
    }
    return slices;
}

QVariantList CashFlowController::biggestRows() const
{
    // MOCK: a plausible "biggest expenses" list derived from the category data
    const int month = m_selectedMonth;
    QVariantList rows;
    auto add = [&](const QString& label, int cat, double amount) {
        rows.append(QVariantMap{
            { "label", label },
            { "category", kCats.at(cat).name },
            { "amount", Fmt::money(amount) },
            { "color", kCats.at(cat).color },
        });
    };
    add("Monthly rent",            kCatRent,      m_categoryData.at(kCatRent).at(month));
    add("Supermarket — largest",   kCatGroceries, m_categoryData.at(kCatGroceries).at(month) * kGroceriesBiggestFactor);
    add("Leisure — largest",       kCatLeisure,   m_categoryData.at(kCatLeisure).at(month) * kLeisureBiggestFactor);
    add("Misc — largest",          kCatOther,     m_categoryData.at(kCatOther).at(month) * kOtherBiggestFactor);
    add("Electricity bill",        kCatUtilities, m_categoryData.at(kCatUtilities).at(month) * kUtilitiesBiggestFactor);
    return rows;
}

QVariantList CashFlowController::flowInputs() const
{
    // MOCK: one salary input; totals match flowOutputs()
    double income = m_incomeValues.at(m_selectedMonth).toDouble();
    return {
        QVariantMap{ { "name", "Salary" }, { "value", income }, { "color", "#a6e3a1" } },
    };
}

QVariantList CashFlowController::flowOutputs() const
{
    QVariantList out;
    const int month = m_selectedMonth;
    double spent = 0;
    for (int catIdx = 0; catIdx < kCategories; ++catIdx) {
        const double value = m_categoryData.at(catIdx).at(month);
        spent += value;
        out.append(QVariantMap{
            { "name", kCats.at(catIdx).name },
            { "value", value },
            { "color", kCats.at(catIdx).color },
        });
    }
    // Leftover = savings — keeps input and output totals equal.
    const double leftover = m_incomeValues.at(month).toDouble() - spent;
    out.append(QVariantMap{
        { "name", "Savings" },
        { "value", qMax(leftover, 0.0) },
        { "color", "#94e2d5" },
    });
    return out;
}

QVariantList CashFlowController::categoryValues(int index) const
{
    QVariantList result;
    if (index < 0 || index >= kCategories) { return result; }
    for (int monthIdx = 0; monthIdx < kMonths; ++monthIdx) {
        result.append(m_categoryData.at(index).at(monthIdx));
    }
    return result;
}
