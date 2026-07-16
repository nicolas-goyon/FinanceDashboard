#include "SavingsController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QDateTime>

SavingsController::SavingsController(QObject* parent)
    : QObject(parent)
{
    // MOCK: monthly balances (Livret A / LDDS / Assurance Vie)
    const double balances[kAccounts][kMonths] = {
        { 15200, 15600, 16000, 16450, 16900, 17300, 17750, 18150, 18550, 18900, 19200, 19500 },
        {  8100,  8300,  8500,  8650,  8800,  9000,  9200,  9350,  9500,  9700,  9850, 10000 },
        { 12100, 12250, 12150, 12400, 12600, 12550, 12800, 13000, 13150, 13400, 13650, 13900 },
    };
    // MOCK: interest / gains per year (2023–2025)
    const double interest[kAccounts][kYears] = {
        { 380, 465, 510 },
        { 190, 240, 265 },
        { 310, 420, 505 },
    };

    for (int a = 0; a < kAccounts; ++a) {
        for (int m = 0; m < kMonths; ++m) {
            m_balances[a][m] = balances[a][m];
            m_balanceMax = qMax(m_balanceMax, balances[a][m]);
        }
        for (int y = 0; y < kYears; ++y) {
            m_interest[a][y] = interest[a][y];
            m_interestMax = qMax(m_interestMax, interest[a][y]);
        }
    }
    m_balanceMax  *= 1.15;
    m_interestMax *= 1.25;

    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-11);
    for (int m = 0; m < kMonths; ++m)
        m_timestamps.append(double(firstMonth.addMonths(m)
                                       .startOfDay().toMSecsSinceEpoch()));
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    const int y0 = today.year() - 3;
    m_yearLabels = { QString::number(y0), QString::number(y0 + 1),
                     QString::number(y0 + 2) };

    // ── KPIs ──────────────────────────────────────────────────────────────────
    // MOCK: "Blended Yield" and "Added This Year" are hardcoded display values
    const double total = balances[0][11] + balances[1][11] + balances[2][11];
    m_kpis = {
        QVariantMap{ { "label", "Total Savings" }, { "value", Fmt::money(total) },
                     { "sub", "" }, { "color", "#cdd6f4" } },
        QVariantMap{ { "label", "Blended Yield" }, { "value", "2.9 %" },
                     { "sub", "across all accounts" }, { "color", "#a6e3a1" } },
        QVariantMap{ { "label", "Added This Year" }, { "value", Fmt::money(4300, true) },
                     { "sub", "deposits, excl. interest" }, { "color", "#a6e3a1" } },
    };

    // ── Ceilings / goals ──────────────────────────────────────────────────────
    auto goal = [](const QString& label, double current, double target,
                   const QString& color) {
        return QVariantMap{
            { "label", label },
            { "currentText", Fmt::money(current) },
            { "targetText",  Fmt::money(target) },
            { "pct", qMin(current / target, 1.0) },
            { "color", color },
        };
    };
    m_goals = {
        goal("Livret A — ceiling",       balances[0][11], 22950, "#89b4fa"),
        goal("LDDS — ceiling",           balances[1][11], 12000, "#a6e3a1"),
        goal("Assurance Vie — personal", balances[2][11], 20000, "#f9e2af"),
    };
}

QVariantList SavingsController::accountValues(int index) const
{
    QVariantList v;
    if (index < 0 || index >= kAccounts) return v;
    for (int m = 0; m < kMonths; ++m)
        v.append(m_balances[index][m]);
    return v;
}

QVariantList SavingsController::interestValues(int index) const
{
    QVariantList v;
    if (index < 0 || index >= kAccounts) return v;
    for (int y = 0; y < kYears; ++y)
        v.append(m_interest[index][y]);
    return v;
}
