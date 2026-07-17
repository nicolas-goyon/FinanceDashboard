#ifndef FINANCEDASHBOARD_CONTROLLERS_OVERVIEW_NETWORTHCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_OVERVIEW_NETWORTHCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── NetWorthController ────────────────────────────────────────────────────────
//
// Backend for qml/pages/OverviewPage.qml (PROJECT.md page 1 — Net Worth).
// MOCK DATA for the UI pass — 12 monthly snapshots split by account type.
// Replace the generator in the .cpp with real DB aggregation later.

class NetWorthController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ label, value, sub, color }] — 4 KPI cards
    Q_PROPERTY(QVariantList kpis READ kpis CONSTANT)
    // [{ label, value, color }] — allocation donut slices (current month)
    Q_PROPERTY(QVariantList allocation READ allocation CONSTANT)
    // Month labels for bar categories ("Jul 25", …)
    Q_PROPERTY(QStringList monthLabels READ monthLabels CONSTANT)
    // Monthly net change split into two bar sets (green gains / red losses)
    Q_PROPERTY(QVariantList netGains  READ netGains  CONSTANT)
    Q_PROPERTY(QVariantList netLosses READ netLosses CONSTANT)
    Q_PROPERTY(double netChangeMax READ netChangeMax CONSTANT)
    Q_PROPERTY(double netChangeMin READ netChangeMin CONSTANT)
    // Stacked area axes
    Q_PROPERTY(double maxTotal  READ maxTotal  CONSTANT)
    Q_PROPERTY(double minTimeMs READ minTimeMs CONSTANT)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs CONSTANT)

public:
    explicit NetWorthController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList kpis()       const { return m_kpis; }
    [[nodiscard]] QVariantList allocation() const { return m_allocation; }
    [[nodiscard]] QStringList  monthLabels() const { return m_monthLabels; }
    [[nodiscard]] QVariantList netGains()   const { return m_netGains; }
    [[nodiscard]] QVariantList netLosses()  const { return m_netLosses; }
    [[nodiscard]] double netChangeMax() const { return m_netChangeMax; }
    [[nodiscard]] double netChangeMin() const { return m_netChangeMin; }
    [[nodiscard]] double maxTotal()  const { return m_maxTotal; }
    [[nodiscard]] double minTimeMs() const { return m_minTimeMs; }
    [[nodiscard]] double maxTimeMs() const { return m_maxTimeMs; }

    // Stacked area boundaries (cumulative, bottom → top: checking, savings, trading)
    Q_INVOKABLE [[nodiscard]] QVariantList timestamps()    const { return m_timestamps; }
    Q_INVOKABLE [[nodiscard]] QVariantList checkingBound() const { return m_checkingBound; }
    Q_INVOKABLE [[nodiscard]] QVariantList savingsBound()  const { return m_savingsBound; }
    Q_INVOKABLE [[nodiscard]] QVariantList tradingBound()  const { return m_tradingBound; }

private:
    QVariantList m_kpis, m_allocation, m_netGains, m_netLosses;
    QVariantList m_timestamps, m_checkingBound, m_savingsBound, m_tradingBound;
    QStringList  m_monthLabels;
    double m_maxTotal = 0, m_minTimeMs = 0, m_maxTimeMs = 0;
    double m_netChangeMax = 0, m_netChangeMin = 0;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_OVERVIEW_NETWORTHCONTROLLER_H
