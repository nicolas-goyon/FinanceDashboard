#include "CashFlowController.h"

#include "controllers/common/NetWorthScope.h"
#include "db/Database.h"
#include "utils/Formatter.h"
#include "utils/Money.h"

#include <QDate>
#include <QDateTime>
#include <QVariantMap>
#include <QVector>
#include <QtMinMax>

#include <algorithm>
#include <array>

namespace {

constexpr double kBarsAxisHeadroom = 1.15; // 15% headroom above the max bar value for the chart axis

// Colours for the categories that have none (Theme palette).
const std::array<const char*, 6> kFallbackColors = {"#89b4fa", "#a6e3a1", "#fab387", "#f9e2af", "#cba6f7", "#f38ba8"};
const QString kOtherColor = QStringLiteral("#6c7086"); // Theme.overlay0

} // namespace

CashFlowController::CashFlowController(QObject* parent)
    : QObject(parent)
{
    reload();
}

void CashFlowController::reload()
{
    m_colors.clear();
    for (const Data::Category& category : Database::instance().loadCategories()) {
        m_colors.insert(category.name, category.color);
    }
    QVector<Data::Transaction> rows;
    const QVector<NetWorthScope::ScopeAccount> scope = NetWorthScope::load();
    for (const NetWorthScope::ScopeAccount& account : scope) {
        rows += account.history.rows;
    }
    m_hasData = !rows.isEmpty();
    m_result = CashFlowCalc::compute(rows, QDate::currentDate());

    m_monthLabels.clear();
    m_incomeValues.clear();
    m_expenseValues.clear();
    m_rollingTimestamps.clear();
    m_rollingValues.clear();
    m_categories.clear();
    m_barsMax = 0;
    m_stackedMax = 0;
    QVariantList timestamps;
    for (int m = 0; m < CashFlowCalc::kMonths; ++m) {
        const QDate month = m_result.months.at(m);
        m_monthLabels.append(month.toString(QStringLiteral("MMM yy")));
        timestamps.append(static_cast<double>(month.startOfDay().toMSecsSinceEpoch()));
        const double income = Money::fromCents(m_result.incomeCents.at(m));
        const double spending = Money::fromCents(m_result.spendingCents.at(m));
        m_incomeValues.append(income);
        m_expenseValues.append(spending);
        m_barsMax = qMax(m_barsMax, qMax(income, spending));
        m_stackedMax = qMax(m_stackedMax, spending);
    }
    m_barsMax = m_barsMax > 0 ? m_barsMax * kBarsAxisHeadroom : 1.0;
    m_stackedMax = m_stackedMax > 0 ? m_stackedMax * kBarsAxisHeadroom : 1.0;
    m_minTimeMs = timestamps.first().toDouble();
    m_maxTimeMs = timestamps.last().toDouble();
    for (qsizetype i = 0; i < m_result.rolling.size(); ++i) {
        m_rollingTimestamps.append(timestamps.at(i + CashFlowCalc::kRollingWindow - 1));
        m_rollingValues.append(m_result.rolling.at(i));
    }
    for (const QString& name : std::as_const(m_result.stackedNames)) {
        m_categories.append(QVariantMap{{"name", name}, {"color", colorOf(name)}});
    }
    emit changed();
    emit monthChanged();
}

QString CashFlowController::colorOf(const QString& category) const
{
    if (category == CashFlowCalc::other() || category == CashFlowCalc::uncategorized() || category.isEmpty()) {
        return kOtherColor;
    }
    const QString color = m_colors.value(category);
    if (!color.isEmpty()) {
        return color;
    }
    return QString::fromLatin1(kFallbackColors.at(qHash(category) % kFallbackColors.size()));
}

void CashFlowController::setSelectedMonth(int month)
{
    if (month < 0 || month >= CashFlowCalc::kMonths || month == m_selectedMonth) {
        return;
    }
    m_selectedMonth = month;
    emit monthChanged();
}

QVector<CashFlowCalc::Part> CashFlowController::monthParts(int month) const
{
    QVector<CashFlowCalc::Part> parts;
    const QHash<QString, qint64>& spending = m_result.byCategory.at(month);
    for (auto it = spending.cbegin(); it != spending.cend(); ++it) {
        parts.append(CashFlowCalc::Part{it.key(), Money::fromCents(it.value()), colorOf(it.key())});
    }
    std::sort(parts.begin(), parts.end(), [](const CashFlowCalc::Part& a, const CashFlowCalc::Part& b) {
        return a.value != b.value ? a.value > b.value : a.name < b.name;
    });
    return parts;
}

QVariantList CashFlowController::monthSlices() const
{
    QVariantList slices;
    for (const CashFlowCalc::Part& part : monthParts(m_selectedMonth)) {
        slices.append(QVariantMap{{"label", part.name}, {"value", part.value}, {"color", part.color}});
    }
    return slices;
}

QVariantList CashFlowController::biggestRows() const
{
    QVariantList rows;
    for (const Data::Transaction& row : m_result.biggest.at(m_selectedMonth)) {
        const QString category = row.category.isEmpty() ? CashFlowCalc::uncategorized() : row.category;
        rows.append(QVariantMap{
            {"label", row.name.isEmpty() ? row.description : row.name},
            {"category", category},
            {"amount", Fmt::money(row.debit)},
            {"color", colorOf(category)},
        });
    }
    return rows;
}

QVariantList CashFlowController::flowInputs() const
{
    return CashFlowCalc::moneyFlow(m_incomeValues.at(m_selectedMonth).toDouble(), monthParts(m_selectedMonth)).inputs;
}

QVariantList CashFlowController::flowOutputs() const
{
    return CashFlowCalc::moneyFlow(m_incomeValues.at(m_selectedMonth).toDouble(), monthParts(m_selectedMonth)).outputs;
}

QVariantList CashFlowController::categoryValues(int index) const
{
    QVariantList result;
    if (index < 0 || index >= m_result.stackedCents.size()) {
        return result;
    }
    for (const qint64 cents : m_result.stackedCents.at(index)) {
        result.append(Money::fromCents(cents));
    }
    return result;
}
