#ifndef FINANCEDASHBOARD_CONTROLLERS_OVERVIEW_NETWORTHCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_OVERVIEW_NETWORTHCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── NetWorthController ───────────────────────────────────────────────────────
//
// Backs qml/pages/OverviewPage.qml (#209): "how am I doing overall?", from
// the accounts of the net worth (#206). The values come from NetWorthCalc
// (balances, valuations, #208); the savings leave out the internal transfers
// (#207). Three groups of accounts (NetWorthScope::Group): day-to-day,
// savings, investments.
//
// reload() reads the database again: the page calls it when it is shown.
// The 12 points of the charts are the ends of the last 12 months (the last
// one is today).
class NetWorthController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // False when no account is in the net worth.
    Q_PROPERTY(bool hasData READ hasData NOTIFY changed)
    // [{ label, value, sub, color }] — 4 KPI cards
    Q_PROPERTY(QVariantList kpis READ kpis NOTIFY changed)
    // [{ label, value, color }] — allocation donut slices (today, values > 0)
    Q_PROPERTY(QVariantList allocation READ allocation NOTIFY changed)
    // Month labels for bar categories ("Jul 25", …)
    Q_PROPERTY(QStringList monthLabels READ monthLabels NOTIFY changed)
    // Monthly change of the net worth, split into two bar sets
    Q_PROPERTY(QVariantList netGains  READ netGains  NOTIFY changed)
    Q_PROPERTY(QVariantList netLosses READ netLosses NOTIFY changed)
    Q_PROPERTY(double netChangeMax READ netChangeMax NOTIFY changed)
    Q_PROPERTY(double netChangeMin READ netChangeMin NOTIFY changed)
    // Stacked area axes
    Q_PROPERTY(double minTotal  READ minTotal  NOTIFY changed)
    Q_PROPERTY(double maxTotal  READ maxTotal  NOTIFY changed)
    Q_PROPERTY(double minTimeMs READ minTimeMs NOTIFY changed)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs NOTIFY changed)

public:
    explicit NetWorthController(QObject* parent = nullptr);

    [[nodiscard]] bool hasData() const { return m_hasData; }
    [[nodiscard]] QVariantList kpis()       const { return m_kpis; }
    [[nodiscard]] QVariantList allocation() const { return m_allocation; }
    [[nodiscard]] QStringList  monthLabels() const { return m_monthLabels; }
    [[nodiscard]] QVariantList netGains()   const { return m_netGains; }
    [[nodiscard]] QVariantList netLosses()  const { return m_netLosses; }
    [[nodiscard]] double netChangeMax() const { return m_netChangeMax; }
    [[nodiscard]] double netChangeMin() const { return m_netChangeMin; }
    [[nodiscard]] double minTotal()  const { return m_minTotal; }
    [[nodiscard]] double maxTotal()  const { return m_maxTotal; }
    [[nodiscard]] double minTimeMs() const { return m_minTimeMs; }
    [[nodiscard]] double maxTimeMs() const { return m_maxTimeMs; }

    // Stacked area boundaries (cumulative, bottom → top: day-to-day,
    // savings, investments).
    Q_INVOKABLE [[nodiscard]] QVariantList timestamps()       const { return m_timestamps; }
    Q_INVOKABLE [[nodiscard]] QVariantList dayToDayBound()    const { return m_dayToDayBound; }
    Q_INVOKABLE [[nodiscard]] QVariantList savingsBound()     const { return m_savingsBound; }
    Q_INVOKABLE [[nodiscard]] QVariantList investmentsBound() const { return m_investmentsBound; }

    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    bool m_hasData = false;
    QVariantList m_kpis, m_allocation, m_netGains, m_netLosses;
    QVariantList m_timestamps, m_dayToDayBound, m_savingsBound, m_investmentsBound;
    QStringList  m_monthLabels;
    double m_minTotal = 0, m_maxTotal = 0, m_minTimeMs = 0, m_maxTimeMs = 0;
    double m_netChangeMax = 0, m_netChangeMin = 0;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_OVERVIEW_NETWORTHCONTROLLER_H
