#include "calc/TransferMatcher.h"

#include "utils/Money.h"

#include <QHash>

#include <algorithm>
#include <cstdlib>

namespace TransferMatcher {

QVector<Pair> find(const QVector<Data::Transaction>& rows, const QSet<int>& scope, int maxDays)
{
    // The credits that can be the other side, by amount in cents.
    QHash<qint64, QVector<const Data::Transaction*>> credits;
    for (const Data::Transaction& row : rows) {
        if (row.transferId == 0 && scope.contains(row.accountId) && row.credit > 0.0 && row.debit <= 0.0) {
            credits[Money::toCents(row.credit)].append(&row);
        }
    }

    QVector<Pair> candidates;
    for (const Data::Transaction& debit : rows) {
        if (debit.transferId != 0 || !scope.contains(debit.accountId) || debit.debit <= 0.0 || debit.credit > 0.0) {
            continue;
        }
        const QDate debitDate = Data::bookingDate(debit);
        const auto found = credits.constFind(Money::toCents(debit.debit));
        if (found == credits.cend() || !debitDate.isValid()) {
            continue;
        }
        for (const Data::Transaction* credit : *found) {
            const QDate creditDate = Data::bookingDate(*credit);
            if (credit->accountId == debit.accountId || !creditDate.isValid()) {
                continue;
            }
            const qint64 days = std::llabs(debitDate.daysTo(creditDate));
            if (days <= maxDays) {
                candidates.append(Pair{debit.id, credit->id, static_cast<int>(days)});
            }
        }
    }

    // The smallest gap first, then the older debit (smaller id), then the
    // older credit: the same rows give the same pairs.
    std::sort(candidates.begin(), candidates.end(), [](const Pair& a, const Pair& b) {
        if (a.days != b.days) {
            return a.days < b.days;
        }
        if (a.debitId != b.debitId) {
            return a.debitId < b.debitId;
        }
        return a.creditId < b.creditId;
    });
    QSet<int> used;
    QVector<Pair> result;
    for (const Pair& pair : candidates) {
        if (used.contains(pair.debitId) || used.contains(pair.creditId)) {
            continue;
        }
        used.insert(pair.debitId);
        used.insert(pair.creditId);
        result.append(pair);
    }
    return result;
}

} // namespace TransferMatcher
