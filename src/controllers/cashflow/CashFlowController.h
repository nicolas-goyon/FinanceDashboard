#ifndef FINANCEDASHBOARD_CONTROLLERS_CASHFLOW_CASHFLOWCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_CASHFLOW_CASHFLOWCONTROLLER_H

#include "calc/CashFlowCalc.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── CashFlowController ───────────────────────────────────────────────────────
//
// Backs qml/pages/CashFlowPage.qml (#210): where the money goes, from the
// rows of the accounts of the net worth, without the internal transfers. The
// figures come from CashFlowCalc; the colours of the categories from the
// categories of the app. reload() reads the database again: the page calls
// it when it is shown.
class CashFlowController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool hasData READ hasData NOTIFY changed)
    Q_PROPERTY(QStringList monthLabels READ monthLabels NOTIFY changed)
    Q_PROPERTY(QVariantList incomeValues  READ incomeValues  NOTIFY changed)
    Q_PROPERTY(QVariantList expenseValues READ expenseValues NOTIFY changed)
    Q_PROPERTY(double barsMax READ barsMax NOTIFY changed)
    Q_PROPERTY(double stackedMax READ stackedMax NOTIFY changed)
    // [{ name, color }] — the kStackedCategories sets of the stacked bars
    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)

    // Selected month (0-11) drives the money flow, the donut and the table
    Q_PROPERTY(int selectedMonth READ selectedMonth WRITE setSelectedMonth NOTIFY monthChanged)
    // [{ label, value, color }] — donut slices for the selected month
    Q_PROPERTY(QVariantList monthSlices READ monthSlices NOTIFY monthChanged)
    // [{ label, category, amount, color }] — biggest expenses of the month
    Q_PROPERTY(QVariantList biggestRows READ biggestRows NOTIFY monthChanged)
    // Money flow of the selected month: income → budget → categories + savings
    Q_PROPERTY(QVariantList flowInputs  READ flowInputs  NOTIFY monthChanged)
    Q_PROPERTY(QVariantList flowOutputs READ flowOutputs NOTIFY monthChanged)

    Q_PROPERTY(double minTimeMs READ minTimeMs NOTIFY changed)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs NOTIFY changed)

public:
    static constexpr int kCategories = CashFlowCalc::kStackedCategories;

    explicit CashFlowController(QObject* parent = nullptr);

    [[nodiscard]] bool hasData() const { return m_hasData; }
    [[nodiscard]] QStringList  monthLabels()   const { return m_monthLabels; }
    [[nodiscard]] QVariantList incomeValues()  const { return m_incomeValues; }
    [[nodiscard]] QVariantList expenseValues() const { return m_expenseValues; }
    [[nodiscard]] double       barsMax()       const { return m_barsMax; }
    [[nodiscard]] double       stackedMax()    const { return m_stackedMax; }
    [[nodiscard]] QVariantList categories()    const { return m_categories; }

    [[nodiscard]] int  selectedMonth() const { return m_selectedMonth; }
    void setSelectedMonth(int month);
    [[nodiscard]] QVariantList monthSlices() const;
    [[nodiscard]] QVariantList biggestRows() const;
    [[nodiscard]] QVariantList flowInputs() const;
    [[nodiscard]] QVariantList flowOutputs() const;

    // Rolling 3-month average of the spending — points for a LineSeries
    // (x = ms since epoch). The line starts at the third month (#56).
    Q_INVOKABLE [[nodiscard]] QVariantList rollingTimestamps() const { return m_rollingTimestamps; }
    Q_INVOKABLE [[nodiscard]] QVariantList rollingValues()     const { return m_rollingValues; }
    [[nodiscard]] double minTimeMs() const { return m_minTimeMs; }
    [[nodiscard]] double maxTimeMs() const { return m_maxTimeMs; }

    // Values of stacked category `index` across the 12 months
    Q_INVOKABLE [[nodiscard]] QVariantList categoryValues(int index) const;

    Q_INVOKABLE void reload();

signals:
    void changed();
    void monthChanged();

private:
    [[nodiscard]] QString colorOf(const QString& category) const;
    [[nodiscard]] QVector<CashFlowCalc::Part> monthParts(int month) const;

    CashFlowCalc::Result m_result;
    QHash<QString, QString> m_colors; // category -> colour
    bool m_hasData = false;
    QStringList  m_monthLabels;
    QVariantList m_incomeValues, m_expenseValues;
    QVariantList m_rollingTimestamps, m_rollingValues;
    QVariantList m_categories;
    double m_barsMax = 0, m_stackedMax = 0, m_minTimeMs = 0, m_maxTimeMs = 0;
    int    m_selectedMonth = CashFlowCalc::kMonths - 1;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_CASHFLOW_CASHFLOWCONTROLLER_H
