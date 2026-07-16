#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── SavingsController ─────────────────────────────────────────────────────────
//
// Backend for qml/pages/SavingsPage.qml (PROJECT.md page 3 — Savings).
// MOCK DATA for the UI pass — 3 accounts (Livret A, LDDS, Assurance Vie).

class SavingsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ label, value, sub, color }] — KPI cards (total savings, blended yield)
    Q_PROPERTY(QVariantList kpis READ kpis CONSTANT)
    // [{ label, currentText, pct, color, targetText }] — ceiling/goal progress
    Q_PROPERTY(QVariantList goals READ goals CONSTANT)
    Q_PROPERTY(QStringList yearLabels READ yearLabels CONSTANT)
    Q_PROPERTY(double interestMax READ interestMax CONSTANT)
    Q_PROPERTY(double balanceMax READ balanceMax CONSTANT)
    Q_PROPERTY(double minTimeMs READ minTimeMs CONSTANT)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs CONSTANT)

public:
    explicit SavingsController(QObject* parent = nullptr);

    QVariantList kpis()  const { return m_kpis; }
    QVariantList goals() const { return m_goals; }
    QStringList yearLabels() const { return m_yearLabels; }
    double interestMax() const { return m_interestMax; }
    double balanceMax()  const { return m_balanceMax; }
    double minTimeMs()   const { return m_minTimeMs; }
    double maxTimeMs()   const { return m_maxTimeMs; }

    // Balance-over-time lines: shared timestamps + per-account values
    // (0 = Livret A, 1 = LDDS, 2 = Assurance Vie — same order as goals)
    Q_INVOKABLE QVariantList timestamps() const { return m_timestamps; }
    Q_INVOKABLE QVariantList accountValues(int index) const;

    // Interest earned per year per account (for grouped BarSets)
    Q_INVOKABLE QVariantList interestValues(int index) const;

private:
    static constexpr int kMonths = 12;
    static constexpr int kAccounts = 3;
    static constexpr int kYears = 3;

    QVariantList m_kpis, m_goals, m_timestamps;
    QStringList  m_yearLabels;
    double m_balances[kAccounts][kMonths] = {};
    double m_interest[kAccounts][kYears]  = {};
    double m_interestMax = 0, m_balanceMax = 0;
    double m_minTimeMs = 0, m_maxTimeMs = 0;
};
