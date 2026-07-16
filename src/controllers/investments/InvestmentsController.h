#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── InvestmentsController ─────────────────────────────────────────────────────
//
// Backend for qml/pages/InvestmentsPage.qml (PROJECT.md page 4 — Investments).
// MOCK DATA for the UI pass. The key chart is portfolio value vs cumulative
// deposits — the gap between the two lines is real P&L.

class InvestmentsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ label, value, sub, color }] — total invested, unrealized P&L, return %
    Q_PROPERTY(QVariantList kpis READ kpis CONSTANT)
    // [{ label, value, color }] — allocation donut
    Q_PROPERTY(QVariantList allocation READ allocation CONSTANT)
    Q_PROPERTY(QStringList monthLabels READ monthLabels CONSTANT)
    // Monthly performance % split into green/red bar sets
    Q_PROPERTY(QVariantList perfGains  READ perfGains  CONSTANT)
    Q_PROPERTY(QVariantList perfLosses READ perfLosses CONSTANT)
    Q_PROPERTY(double perfMax READ perfMax CONSTANT)
    Q_PROPERTY(double perfMin READ perfMin CONSTANT)
    Q_PROPERTY(double valueMax READ valueMax CONSTANT)
    Q_PROPERTY(double minTimeMs READ minTimeMs CONSTANT)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs CONSTANT)

public:
    explicit InvestmentsController(QObject* parent = nullptr);

    QVariantList kpis()       const { return m_kpis; }
    QVariantList allocation() const { return m_allocation; }
    QStringList  monthLabels() const { return m_monthLabels; }
    QVariantList perfGains()  const { return m_perfGains; }
    QVariantList perfLosses() const { return m_perfLosses; }
    double perfMax()  const { return m_perfMax; }
    double perfMin()  const { return m_perfMin; }
    double valueMax() const { return m_valueMax; }
    double minTimeMs() const { return m_minTimeMs; }
    double maxTimeMs() const { return m_maxTimeMs; }

    // Portfolio value vs cumulative deposits (two LineSeries)
    Q_INVOKABLE QVariantList timestamps()      const { return m_timestamps; }
    Q_INVOKABLE QVariantList portfolioValues() const { return m_portfolioValues; }
    Q_INVOKABLE QVariantList depositValues()   const { return m_depositValues; }

private:
    QVariantList m_kpis, m_allocation;
    QVariantList m_timestamps, m_portfolioValues, m_depositValues;
    QVariantList m_perfGains, m_perfLosses;
    QStringList  m_monthLabels;
    double m_perfMax = 0, m_perfMin = 0, m_valueMax = 0;
    double m_minTimeMs = 0, m_maxTimeMs = 0;
};
