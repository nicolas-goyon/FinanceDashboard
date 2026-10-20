#ifndef FINANCEDASHBOARD_CONTROLLERS_CASHFLOW_CASHFLOWCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_CASHFLOW_CASHFLOWCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <array>

// ── CashFlowController ────────────────────────────────────────────────────────
//
// Backend for qml/pages/CashFlowPage.qml (PROJECT.md page 2 — Cash Flow).
// MOCK DATA for the UI pass — 12 months of income/expenses and 6 spending
// categories. Rolling averages and per-month aggregation are computed here.

class CashFlowController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QStringList monthLabels READ monthLabels CONSTANT)
    Q_PROPERTY(QVariantList incomeValues  READ incomeValues  CONSTANT)
    Q_PROPERTY(QVariantList expenseValues READ expenseValues CONSTANT)
    Q_PROPERTY(double barsMax READ barsMax CONSTANT)
    // [{ name, color }] — the 6 spending categories
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)

    // Selected month (0-11) drives the donut + biggest-expenses table
    Q_PROPERTY(int selectedMonth READ selectedMonth WRITE setSelectedMonth NOTIFY monthChanged)
    // [{ label, value, color }] — donut slices for the selected month
    Q_PROPERTY(QVariantList monthSlices READ monthSlices NOTIFY monthChanged)
    // [{ label, category, amount, color }] — biggest expenses of the month
    Q_PROPERTY(QVariantList biggestRows READ biggestRows NOTIFY monthChanged)

    // Money flow (Sankey) of the selected month:
    // inputs (salary, an overspend) → month budget → outputs (categories +
    // savings leftover). [{ name, value, color }] — totals of both sides are
    // equal (see moneyFlow())
    Q_PROPERTY(QVariantList flowInputs  READ flowInputs  NOTIFY monthChanged)
    Q_PROPERTY(QVariantList flowOutputs READ flowOutputs NOTIFY monthChanged)

public:
    static constexpr int kCategories = 6;

    explicit CashFlowController(QObject* parent = nullptr);

    [[nodiscard]] QStringList  monthLabels()   const { return m_monthLabels; }
    [[nodiscard]] QVariantList incomeValues()  const { return m_incomeValues; }
    [[nodiscard]] QVariantList expenseValues() const { return m_expenseValues; }
    [[nodiscard]] double       barsMax()       const { return m_barsMax; }
    [[nodiscard]] QVariantList categories()    const { return m_categories; }

    [[nodiscard]] int  selectedMonth() const { return m_selectedMonth; }
    void setSelectedMonth(int month);
    [[nodiscard]] QVariantList monthSlices() const;
    [[nodiscard]] QVariantList biggestRows() const;
    [[nodiscard]] QVariantList flowInputs() const;
    [[nodiscard]] QVariantList flowOutputs() const;

    // Rolling 3-month average of expenses — points for a LineSeries
    // (x = ms since epoch, aligned with the month buckets). The line starts at
    // the third month: the first two months have no 3-month window (#56).
    Q_INVOKABLE [[nodiscard]] QVariantList rollingTimestamps() const { return m_rollingTimestamps; }
    Q_INVOKABLE [[nodiscard]] QVariantList rollingValues()     const { return m_rollingValues; }
    Q_PROPERTY(double minTimeMs READ minTimeMs CONSTANT)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs CONSTANT)
    [[nodiscard]] double minTimeMs() const { return m_minTimeMs; }
    [[nodiscard]] double maxTimeMs() const { return m_maxTimeMs; }

    // Values of category `index` across all 12 months (for stacked BarSets)
    Q_INVOKABLE [[nodiscard]] QVariantList categoryValues(int index) const;

    // One month of the money flow: [{ name, value, color }] for each side,
    // with equal totals. What is left of the income goes out as "Savings".
    // Spending above the income comes in as "Overspend" (taken from the
    // savings), so the outputs do not add up to more than the inputs (#42).
    // A part with no amount is left out. Public for the tests.
    struct FlowSides {
        QVariantList inputs;
        QVariantList outputs;
    };
    [[nodiscard]] static FlowSides moneyFlow(double income, const std::array<double, kCategories>& spending);

signals:
    void monthChanged();

private:
    static constexpr int kMonths = 12;

    QStringList  m_monthLabels;
    QVariantList m_incomeValues, m_expenseValues;
    QVariantList m_timestamps, m_rollingTimestamps, m_rollingValues;
    QVariantList m_categories;
    std::array<std::array<double, kMonths>, kCategories> m_categoryData = {};
    // The spending of each category in `month` (0-11).
    [[nodiscard]] std::array<double, kCategories> monthSpending(int month) const;
    double m_barsMax = 0, m_minTimeMs = 0, m_maxTimeMs = 0;
    int    m_selectedMonth = kMonths - 1;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_CASHFLOW_CASHFLOWCONTROLLER_H
