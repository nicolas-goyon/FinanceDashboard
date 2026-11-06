#include "SavingsController.h"

#include "calc/NetWorthCalc.h"
#include "calc/SavingsCalc.h"
#include "controllers/common/NetWorthScope.h"
#include "utils/Formatter.h"
#include "utils/Money.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include <QtMinMax>

#include <algorithm>
#include <array>

namespace {
constexpr double kBalanceAxisHeadroom = 1.15;  // 15% headroom above the max balance for the chart axis
constexpr double kInterestAxisHeadroom = 1.25; // 25% headroom above the max interest for the chart axis
constexpr double kPercent = 100.0;
// Theme.blue, green, yellow, peach: the colours of the slots.
const std::array<const char*, SavingsController::kSlots> kSlotColors = {"#89b4fa", "#a6e3a1", "#f9e2af", "#fab387"};
const QString kKpiText = QStringLiteral("#cdd6f4");
const QString kGreen = QStringLiteral("#a6e3a1");
} // namespace

SavingsController::SavingsController(QObject* parent)
    : QObject(parent)
{
    reload();
}

void SavingsController::reload()
{
    QVector<NetWorthScope::ScopeAccount> savings;
    for (const NetWorthScope::ScopeAccount& account : NetWorthScope::load()) {
        if (account.group == NetWorthScope::Group::Savings) {
            savings.append(account);
        }
    }
    m_hasData = !savings.isEmpty();
    const QDate today = QDate::currentDate();

    // The accounts with the highest balance first.
    std::sort(savings.begin(), savings.end(), [&today](const auto& a, const auto& b) {
        return NetWorthCalc::valueCentsAt(a.history, today) > NetWorthCalc::valueCentsAt(b.history, today);
    });

    // ── KPIs ──────────────────────────────────────────────────────────────────
    qint64 totalCents = 0;
    qint64 interestThisYear = 0;
    const QDate yearStart(today.year(), 1, 1);
    for (const NetWorthScope::ScopeAccount& account : savings) {
        totalCents += NetWorthCalc::valueCentsAt(account.history, today);
        for (const Data::Transaction& row : account.history.rows) {
            const QDate booked = Data::bookingDate(row);
            if (booked.isValid() && booked >= yearStart && booked <= today && SavingsCalc::isInterest(row)) {
                interestThisYear += Money::toCents(row.credit);
            }
        }
    }
    const qint64 startOfYear = NetWorthCalc::period(NetWorthScope::histories(savings), yearStart, today).startValueCents;
    const qint64 added = totalCents - startOfYear - interestThisYear;
    m_kpis = {
        QVariantMap{{"label", tr("Total Savings")}, {"value", Fmt::money(Money::fromCents(totalCents))},
                    {"sub", tr("%1 account(s)").arg(savings.size())}, {"color", kKpiText}},
        QVariantMap{{"label", tr("Added This Year")}, {"value", Fmt::money(Money::fromCents(added), true)},
                    {"sub", tr("deposits − withdrawals since 1 January")},
                    {"color", added >= 0 ? kGreen : QStringLiteral("#f38ba8")}},
        QVariantMap{{"label", tr("Interest This Year")},
                    {"value", Fmt::money(Money::fromCents(interestThisYear))},
                    {"sub", tr("the rows of interest")}, {"color", kGreen}},
    };

    // ── Ceilings ─────────────────────────────────────────────────────────────
    m_goals.clear();
    for (qsizetype i = 0; i < savings.size(); ++i) {
        const qint64 ceiling = SavingsCalc::ceilingCents(savings.at(i).account.type);
        if (ceiling <= 0) {
            continue;
        }
        const qint64 value = NetWorthCalc::valueCentsAt(savings.at(i).history, today);
        const double pct = qBound(0.0, static_cast<double>(value) / static_cast<double>(ceiling), 1.0);
        m_goals.append(QVariantMap{
            {"label", QStringLiteral("%1 (%2 %)").arg(savings.at(i).account.name).arg(pct * kPercent, 0, 'f', 0)},
            {"currentText", Fmt::money(Money::fromCents(value))},
            {"targetText", Fmt::money(Money::fromCents(ceiling))},
            {"pct", pct},
            {"color", QString::fromLatin1(kSlotColors.at(static_cast<std::size_t>(i % kSlots)))},
        });
    }

    // ── Slots: balance lines and interest bars ───────────────────────────────
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-(kMonths - 1));
    m_timestamps.clear();
    for (int m = 0; m < kMonths; ++m) {
        m_timestamps.append(static_cast<double>(firstMonth.addMonths(m).startOfDay().toMSecsSinceEpoch()));
    }
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();
    m_yearLabels.clear();
    for (int y = kYears - 1; y >= 0; --y) {
        m_yearLabels.append(QString::number(today.year() - y));
    }
    m_slots.clear();
    m_balances = QVector<QVariantList>(kSlots);
    m_interest = QVector<QVariantList>(kSlots);
    m_balanceMax = 0;
    m_interestMax = 0;
    for (int slot = 0; slot < kSlots; ++slot) {
        if (slot >= savings.size()) {
            m_slots.append(QVariantMap{{"name", QString()}, {"color", QString::fromLatin1(kSlotColors.at(slot))}});
            continue;
        }
        const NetWorthScope::ScopeAccount& account = savings.at(slot);
        m_slots.append(QVariantMap{{"name", account.account.name}, {"color", QString::fromLatin1(kSlotColors.at(slot))}});
        for (int m = 0; m < kMonths; ++m) {
            const QDate month = firstMonth.addMonths(m);
            const QDate end = m == kMonths - 1 ? today : month.addMonths(1).addDays(-1);
            const double value = Money::fromCents(NetWorthCalc::valueCentsAt(account.history, end));
            m_balances[slot].append(value);
            m_balanceMax = qMax(m_balanceMax, value);
        }
        for (int y = kYears - 1; y >= 0; --y) {
            const int year = today.year() - y;
            qint64 cents = 0;
            for (const Data::Transaction& row : account.history.rows) {
                const QDate booked = Data::bookingDate(row);
                if (booked.isValid() && booked.year() == year && SavingsCalc::isInterest(row)) {
                    cents += Money::toCents(row.credit);
                }
            }
            m_interest[slot].append(Money::fromCents(cents));
            m_interestMax = qMax(m_interestMax, Money::fromCents(cents));
        }
    }
    m_balanceMax = m_balanceMax > 0 ? m_balanceMax * kBalanceAxisHeadroom : 1.0;
    m_interestMax = m_interestMax > 0 ? m_interestMax * kInterestAxisHeadroom : 1.0;
    emit changed();
}

QVariantList SavingsController::accountValues(int index) const
{
    return index >= 0 && index < m_balances.size() ? m_balances.at(index) : QVariantList();
}

QVariantList SavingsController::interestValues(int index) const
{
    return index >= 0 && index < m_interest.size() ? m_interest.at(index) : QVariantList();
}
