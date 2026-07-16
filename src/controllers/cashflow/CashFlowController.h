#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

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
    // inputs → month budget → outputs (categories + savings leftover)
    // [{ name, value, color }] — totals of both sides are equal
    Q_PROPERTY(QVariantList flowInputs  READ flowInputs  NOTIFY monthChanged)
    Q_PROPERTY(QVariantList flowOutputs READ flowOutputs NOTIFY monthChanged)

public:
    explicit CashFlowController(QObject* parent = nullptr);

    QStringList  monthLabels()   const { return m_monthLabels; }
    QVariantList incomeValues()  const { return m_incomeValues; }
    QVariantList expenseValues() const { return m_expenseValues; }
    double       barsMax()       const { return m_barsMax; }
    QVariantList categories()    const { return m_categories; }

    int  selectedMonth() const { return m_selectedMonth; }
    void setSelectedMonth(int month);
    QVariantList monthSlices() const;
    QVariantList biggestRows() const;
    QVariantList flowInputs() const;
    QVariantList flowOutputs() const;

    // Rolling 3-month average of expenses — points for a LineSeries
    // (x = ms since epoch, aligned with the month buckets)
    Q_INVOKABLE QVariantList rollingTimestamps() const { return m_timestamps; }
    Q_INVOKABLE QVariantList rollingValues()     const { return m_rollingValues; }
    Q_PROPERTY(double minTimeMs READ minTimeMs CONSTANT)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs CONSTANT)
    double minTimeMs() const { return m_minTimeMs; }
    double maxTimeMs() const { return m_maxTimeMs; }

    // Values of category `index` across all 12 months (for stacked BarSets)
    Q_INVOKABLE QVariantList categoryValues(int index) const;

signals:
    void monthChanged();

private:
    static constexpr int kMonths = 12;
    static constexpr int kCategories = 6;

    QStringList  m_monthLabels;
    QVariantList m_incomeValues, m_expenseValues;
    QVariantList m_timestamps, m_rollingValues;
    QVariantList m_categories;
    double m_categoryData[kCategories][kMonths] = {};
    double m_barsMax = 0, m_minTimeMs = 0, m_maxTimeMs = 0;
    int    m_selectedMonth = kMonths - 1;
};
