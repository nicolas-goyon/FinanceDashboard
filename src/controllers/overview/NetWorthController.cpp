#include "NetWorthController.h"

#include "calc/NetWorthCalc.h"
#include "controllers/common/NetWorthScope.h"
#include "utils/Formatter.h"
#include "utils/Money.h"

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QtMinMax>

#include <array>

namespace {

QVariantMap kpi(const QString& label, const QString& value, const QString& sub = {},
                const QString& color = QStringLiteral("#cdd6f4"))
{
    return { { "label", label }, { "value", value }, { "sub", sub }, { "color", color } };
}

constexpr int kMonthsShown = 12;
constexpr double kAxisHeadroom = 1.12;       // 12% headroom above the max total for the chart axis
constexpr double kNetChangeAxisScale = 1.2;  // scales the net-change axis bounds past the actual min/max
constexpr double kNetChangeAxisPadding = 1;  // extra flat padding on the net-change axis bounds
constexpr double kPercent = 100.0;

const QString kGreen = QStringLiteral("#a6e3a1"); // Theme.green
const QString kRed = QStringLiteral("#f38ba8");   // Theme.red
const QString kBlue = QStringLiteral("#89b4fa");  // Theme.blue
const QString kMauve = QStringLiteral("#cba6f7"); // Theme.mauve

QString signedPercent(qint64 change, qint64 base)
{
    if (base <= 0) {
        return QString();
    }
    const double pct = static_cast<double>(change) / static_cast<double>(base) * kPercent;
    return QStringLiteral("%1%2 %").arg(pct >= 0 ? QStringLiteral("+") : QString()).arg(pct, 0, 'f', 1);
}

QString changeText(qint64 change, qint64 base)
{
    const QString pct = signedPercent(change, base);
    const QString money = Fmt::money(Money::fromCents(change), true);
    return pct.isEmpty() ? money : QStringLiteral("%1  (%2)").arg(money, pct);
}

} // namespace

NetWorthController::NetWorthController(QObject* parent)
    : QObject(parent)
{
    reload();
}

void NetWorthController::reload()
{
    const QVector<NetWorthScope::ScopeAccount> scope = NetWorthScope::load();
    m_hasData = !scope.isEmpty();

    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-(kMonthsShown - 1));
    // The end of each month shown; the last one is today. One more point
    // before the first month gives the change of the first month.
    QVector<QDate> ends;
    ends.append(firstMonth.addDays(-1));
    for (int i = 0; i < kMonthsShown; ++i) {
        const QDate month = firstMonth.addMonths(i);
        ends.append(i == kMonthsShown - 1 ? today : month.addMonths(1).addDays(-1));
    }

    // Values in cents: [point][group].
    QVector<std::array<qint64, 3>> values(ends.size(), {0, 0, 0});
    for (const NetWorthScope::ScopeAccount& account : scope) {
        const auto group = static_cast<std::size_t>(account.group);
        for (qsizetype p = 0; p < ends.size(); ++p) {
            values[p].at(group) += NetWorthCalc::valueCentsAt(account.history, ends.at(p));
        }
    }
    const auto total = [&values](qsizetype p) { return values.at(p)[0] + values.at(p)[1] + values.at(p)[2]; };

    m_timestamps.clear();
    m_dayToDayBound.clear();
    m_savingsBound.clear();
    m_investmentsBound.clear();
    m_monthLabels.clear();
    m_netGains.clear();
    m_netLosses.clear();
    m_minTotal = 0;
    m_maxTotal = 0;
    m_netChangeMax = 0;
    m_netChangeMin = 0;
    for (int i = 0; i < kMonthsShown; ++i) {
        const qsizetype p = i + 1;
        const QDate month = firstMonth.addMonths(i);
        const double dayToDay = Money::fromCents(values.at(p)[0]);
        const double savings = dayToDay + Money::fromCents(values.at(p)[1]);
        const double investments = savings + Money::fromCents(values.at(p)[2]);
        m_timestamps.append(static_cast<double>(month.startOfDay().toMSecsSinceEpoch()));
        m_dayToDayBound.append(dayToDay);
        m_savingsBound.append(savings);
        m_investmentsBound.append(investments);
        m_monthLabels.append(month.toString(QStringLiteral("MMM yy")));
        m_maxTotal = qMax(m_maxTotal, qMax(investments, qMax(savings, dayToDay)));
        m_minTotal = qMin(m_minTotal, qMin(dayToDay, qMin(savings, investments)));

        const double diff = Money::fromCents(total(p) - total(p - 1));
        m_netGains.append(diff >= 0 ? diff : 0.0);
        m_netLosses.append(diff < 0 ? diff : 0.0);
        m_netChangeMax = qMax(m_netChangeMax, diff);
        m_netChangeMin = qMin(m_netChangeMin, diff);
    }
    m_maxTotal = m_maxTotal > 0 ? m_maxTotal * kAxisHeadroom : 1.0;
    m_minTotal = m_minTotal < 0 ? m_minTotal * kAxisHeadroom : 0.0;
    m_netChangeMax = (m_netChangeMax * kNetChangeAxisScale) + kNetChangeAxisPadding;
    m_netChangeMin = (m_netChangeMin * kNetChangeAxisScale) - kNetChangeAxisPadding;
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    // ── KPIs ──────────────────────────────────────────────────────────────────
    const qsizetype last = ends.size() - 1;
    const qint64 netToday = total(last);
    const qint64 monthChange = netToday - total(last - 1);
    const qint64 yearAgo = NetWorthCalc::period(NetWorthScope::histories(scope), today.addYears(-1).addDays(1), today)
                               .startValueCents;
    const qint64 yearChange = netToday - yearAgo;
    const NetWorthCalc::Flows month =
        NetWorthCalc::flows(NetWorthScope::histories(scope), QDate(today.year(), today.month(), 1), today);
    const QString savingsRate =
        month.incomeCents > 0
            ? QStringLiteral("%1 %").arg(static_cast<double>(month.savingsCents()) / static_cast<double>(month.incomeCents)
                                             * kPercent, 0, 'f', 1)
            : QStringLiteral("—");

    m_kpis = {
        kpi(QStringLiteral("Total Net Worth"), Fmt::money(Money::fromCents(netToday)),
            QStringLiteral("%1 account(s)").arg(scope.size())),
        kpi(QStringLiteral("This Month"), changeText(monthChange, total(last - 1)),
            QStringLiteral("since ") + Fmt::dateOrDash(ends.at(last - 1)), monthChange >= 0 ? kGreen : kRed),
        kpi(QStringLiteral("Over 12 Months"), changeText(yearChange, yearAgo),
            QStringLiteral("since ") + Fmt::dateOrDash(today.addYears(-1)), yearChange >= 0 ? kGreen : kRed),
        kpi(QStringLiteral("Savings Rate"), savingsRate,
            QStringLiteral("saved %1 this month").arg(Fmt::money(Money::fromCents(month.savingsCents()))),
            month.savingsCents() >= 0 ? kGreen : kRed),
    };

    // ── Allocation today (only the groups with a positive value) ────────────
    m_allocation.clear();
    const std::array<QString, 3> names = {QStringLiteral("Day-to-day"), QStringLiteral("Savings"),
                                          QStringLiteral("Investments")};
    const std::array<QString, 3> colors = {kBlue, kGreen, kMauve};
    for (std::size_t g = 0; g < names.size(); ++g) {
        const qint64 value = values.at(last).at(g);
        if (value > 0) {
            m_allocation.append(QVariantMap{
                {"label", names.at(g)}, {"value", Money::fromCents(value)}, {"color", colors.at(g)}});
        }
    }
    emit changed();
}
