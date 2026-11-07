#ifndef FINANCEDASHBOARD_CONTROLLERS_SAVINGS_SAVINGSCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_SAVINGS_SAVINGSCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── SavingsController ────────────────────────────────────────────────────────
//
// Backs qml/pages/SavingsPage.qml (#211): the savings accounts of the net
// worth (NetWorthScope::Group::Savings: Livret A, LDDS, PEL…). Balance over
// time (NetWorthCalc), progress toward the ceiling of each regulated account
// (SavingsCalc::ceilingCents()), and the interest of each year
// (SavingsCalc::isInterest()). reload() reads the database again: the page
// calls it when it is shown.
//
// The charts have kSlots series: the kSlots accounts with the highest
// balance. The other accounts count in the KPIs only.
class SavingsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool hasData READ hasData NOTIFY changed)
    // [{ label, value, sub, color }] — KPI cards
    Q_PROPERTY(QVariantList kpis READ kpis NOTIFY changed)
    // [{ label, currentText, pct, color, targetText }] — ceiling progress
    Q_PROPERTY(QVariantList goals READ goals NOTIFY changed)
    // [{ name, color }] — kSlots entries; an unused slot has an empty name
    Q_PROPERTY(QVariantList seriesSlots READ seriesSlots NOTIFY changed)
    Q_PROPERTY(QStringList yearLabels READ yearLabels NOTIFY changed)
    Q_PROPERTY(double interestMax READ interestMax NOTIFY changed)
    Q_PROPERTY(double balanceMax READ balanceMax NOTIFY changed)
    Q_PROPERTY(double minTimeMs READ minTimeMs NOTIFY changed)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs NOTIFY changed)

public:
    static constexpr int kSlots = 4;

    explicit SavingsController(QObject* parent = nullptr);

    [[nodiscard]] bool hasData() const { return m_hasData; }
    [[nodiscard]] QVariantList kpis()  const { return m_kpis; }
    [[nodiscard]] QVariantList goals() const { return m_goals; }
    [[nodiscard]] QVariantList seriesSlots() const { return m_slots; }
    [[nodiscard]] QStringList yearLabels() const { return m_yearLabels; }
    [[nodiscard]] double interestMax() const { return m_interestMax; }
    [[nodiscard]] double balanceMax()  const { return m_balanceMax; }
    [[nodiscard]] double minTimeMs()   const { return m_minTimeMs; }
    [[nodiscard]] double maxTimeMs()   const { return m_maxTimeMs; }

    // Balance-over-time lines: shared timestamps + the values of slot `index`.
    Q_INVOKABLE [[nodiscard]] QVariantList timestamps() const { return m_timestamps; }
    Q_INVOKABLE [[nodiscard]] QVariantList accountValues(int index) const;
    // Interest of each year (yearLabels) for slot `index`.
    Q_INVOKABLE [[nodiscard]] QVariantList interestValues(int index) const;

    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    static constexpr int kMonths = 12;
    static constexpr int kYears = 3;

    bool m_hasData = false;
    QVariantList m_kpis, m_goals, m_slots, m_timestamps;
    QStringList  m_yearLabels;
    QVector<QVariantList> m_balances; // [slot] 12 values
    QVector<QVariantList> m_interest; // [slot] kYears values
    double m_interestMax = 0, m_balanceMax = 0;
    double m_minTimeMs = 0, m_maxTimeMs = 0;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_SAVINGS_SAVINGSCONTROLLER_H
