#include "calc/NetWorthCalc.h"

#include "utils/Money.h"

#include <algorithm>
#include <optional>

namespace NetWorthCalc {

namespace {

qint64 netCents(const Data::Transaction& row)
{
    return Money::toCents(row.credit) - Money::toCents(row.debit);
}

} // namespace

qint64 valueCentsAt(const AccountHistory& account, const QDate& date)
{
    if (!date.isValid()) {
        return 0;
    }
    std::optional<Data::AccountBalanceCheckpoint> checkpoint;
    for (const Data::AccountBalanceCheckpoint& candidate : account.checkpoints) {
        if (candidate.date.isValid() && candidate.date <= date && (!checkpoint || candidate.date > checkpoint->date)) {
            checkpoint = candidate;
        }
    }
    if (checkpoint) {
        qint64 value = Money::toCents(checkpoint->balance);
        for (const Data::Transaction& row : account.rows) {
            const QDate booked = Data::bookingDate(row);
            if (booked.isValid() && booked > checkpoint->date && booked <= date) {
                value += netCents(row);
            }
        }
        return value;
    }

    // No checkpoint: start from the balance before the first row.
    const Data::Transaction* first = nullptr;
    for (const Data::Transaction& row : account.rows) {
        const QDate booked = Data::bookingDate(row);
        if (!booked.isValid()) {
            continue;
        }
        if (first == nullptr || booked < Data::bookingDate(*first)
            || (booked == Data::bookingDate(*first) && row.id < first->id)) {
            first = &row;
        }
    }
    if (first == nullptr || Data::bookingDate(*first) > date) {
        return 0;
    }
    qint64 value = Money::toCents(first->balanceAfter) - netCents(*first);
    for (const Data::Transaction& row : account.rows) {
        const QDate booked = Data::bookingDate(row);
        if (booked.isValid() && booked <= date) {
            value += netCents(row);
        }
    }
    return value;
}

Flows flows(const QVector<AccountHistory>& accounts, const QDate& from, const QDate& to)
{
    Flows result;
    for (const AccountHistory& account : accounts) {
        for (const Data::Transaction& row : account.rows) {
            const QDate booked = Data::bookingDate(row);
            if (!booked.isValid() || booked < from || booked > to || Data::isInternalTransfer(row)) {
                continue;
            }
            result.incomeCents += Money::toCents(row.credit);
            result.spendingCents += Money::toCents(row.debit);
        }
    }
    return result;
}

Period period(const QVector<AccountHistory>& accounts, const QDate& from, const QDate& to)
{
    Period result;
    result.flows = flows(accounts, from, to);
    for (const AccountHistory& account : accounts) {
        result.startValueCents += valueCentsAt(account, from.addDays(-1));
        result.endValueCents += valueCentsAt(account, to);
    }
    return result;
}

} // namespace NetWorthCalc
