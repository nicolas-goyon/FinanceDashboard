#include "calc/CashFlowCalc.h"

#include "utils/Money.h"

#include <QVariantMap>

#include <algorithm>

namespace CashFlowCalc {

QString uncategorized()
{
    return QStringLiteral("Uncategorized");
}

QString other()
{
    return QStringLiteral("Other");
}

Result compute(const QVector<Data::Transaction>& rows, const QDate& today)
{
    Result result;
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-(kMonths - 1));
    for (int m = 0; m < kMonths; ++m) {
        result.months.append(firstMonth.addMonths(m));
    }
    result.incomeCents.fill(0, kMonths);
    result.spendingCents.fill(0, kMonths);
    result.byCategory.resize(kMonths);
    result.biggest.resize(kMonths);

    QHash<QString, qint64> yearByCategory;
    for (const Data::Transaction& row : rows) {
        const QDate booked = Data::bookingDate(row);
        if (!booked.isValid() || booked < firstMonth || Data::isInternalTransfer(row)) {
            continue;
        }
        const int m = ((booked.year() - firstMonth.year()) * 12) + booked.month() - firstMonth.month();
        if (m < 0 || m >= kMonths) {
            continue;
        }
        result.incomeCents[m] += Money::toCents(row.credit);
        const qint64 spent = Money::toCents(row.debit);
        if (spent > 0) {
            result.spendingCents[m] += spent;
            const QString category = row.category.isEmpty() ? uncategorized() : row.category;
            result.byCategory[m][category] += spent;
            yearByCategory[category] += spent;
            result.biggest[m].append(row);
        }
    }

    for (QVector<Data::Transaction>& biggest : result.biggest) {
        std::sort(biggest.begin(), biggest.end(), [](const Data::Transaction& a, const Data::Transaction& b) {
            return a.debit != b.debit ? a.debit > b.debit : a.id < b.id;
        });
        if (biggest.size() > kBiggestRows) {
            biggest.resize(kBiggestRows);
        }
    }

    for (int m = kRollingWindow - 1; m < kMonths; ++m) {
        qint64 sum = 0;
        for (int k = m - (kRollingWindow - 1); k <= m; ++k) {
            sum += result.spendingCents.at(k);
        }
        result.rolling.append(Money::fromCents(sum) / kRollingWindow);
    }

    // The top 5 categories, then "Other".
    QVector<std::pair<QString, qint64>> ranked;
    for (auto it = yearByCategory.cbegin(); it != yearByCategory.cend(); ++it) {
        ranked.append({it.key(), it.value()});
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    const qsizetype named = std::min<qsizetype>(ranked.size(), kStackedCategories - 1);
    for (qsizetype i = 0; i < named; ++i) {
        result.stackedNames.append(ranked.at(i).first);
    }
    while (result.stackedNames.size() < kStackedCategories - 1) {
        result.stackedNames.append(QString());
    }
    result.stackedNames.append(ranked.size() > named ? other() : QString());
    result.stackedCents = QVector<QVector<qint64>>(kStackedCategories, QVector<qint64>(kMonths, 0));
    for (int m = 0; m < kMonths; ++m) {
        for (auto it = result.byCategory.at(m).cbegin(); it != result.byCategory.at(m).cend(); ++it) {
            const qsizetype index = result.stackedNames.indexOf(it.key());
            const qsizetype slot = index >= 0 && index < kStackedCategories - 1 ? index : kStackedCategories - 1;
            result.stackedCents[slot][m] += it.value();
        }
    }
    return result;
}

FlowSides moneyFlow(double income, const QVector<Part>& spending)
{
    FlowSides sides;
    if (income > 0.0) {
        sides.inputs.append(QVariantMap{{"name", QStringLiteral("Income")}, {"value", income}, {"color", "#a6e3a1"}});
    }
    // Only the parts that are drawn count: a negative value is left out of
    // the chart, so it is left out of the total too.
    double spent = 0;
    for (const Part& part : spending) {
        if (part.value > 0.0) {
            spent += part.value;
            sides.outputs.append(QVariantMap{{"name", part.name}, {"value", part.value}, {"color", part.color}});
        }
    }
    // The leftover goes to the savings; an overspend comes from them. Both
    // keep the totals of the two sides equal.
    const double leftover = income - spent;
    if (leftover > 0.0) {
        sides.outputs.append(QVariantMap{{"name", QStringLiteral("Savings")}, {"value", leftover}, {"color", "#94e2d5"}});
    } else if (leftover < 0.0) {
        sides.inputs.append(QVariantMap{{"name", QStringLiteral("Overspend")}, {"value", -leftover}, {"color", "#f38ba8"}});
    }
    return sides;
}

} // namespace CashFlowCalc
