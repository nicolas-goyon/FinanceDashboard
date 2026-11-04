#ifndef FINANCEDASHBOARD_CALC_NETWORTHCALC_H
#define FINANCEDASHBOARD_CALC_NETWORTHCALC_H

#include "models/Account.h"
#include "models/Transaction.h"

#include <QDate>
#include <QVector>

// ── NetWorthCalc ─────────────────────────────────────────────────────────────
//
// The value of an account at a date, and the flows of a period (#208, #212).
// All amounts are whole cents (#33). No database: the controllers load the
// rows and give them here, so calc_test can check the figures.
//
// Value of an account at the end of day D:
//   - c = the latest balance checkpoint (a valuation for an investment
//     account) on or before D. Its balance is the value at the end of its day.
//   - value = c + the rows booked after c.date and on or before D.
//   - With no checkpoint before D: the balance before the first row (its
//     balanceAfter minus its own amount) + the rows on or before D. No row
//     before D either: 0.
// So between two valuations of an investment account, the value is the last
// valuation plus the deposits and withdrawals after it.
//
// Flows of a period, for the accounts of the net worth: the rows that are not
// internal transfers (#207). Income = their credits, spending = their debits,
// savings = income - spending.
//
// Change of the net worth = value at the end - value before the start
//                         = savings + gains.
// The gains (or losses) are the rest: the valuations of the investments, the
// interest that is not in a row, a correction of a checkpoint, and a transfer
// whose other side is not imported.
namespace NetWorthCalc {

struct AccountHistory {
    int accountId = -1;
    QVector<Data::Transaction> rows;                       // any order
    QVector<Data::AccountBalanceCheckpoint> checkpoints;   // any order
};

[[nodiscard]] qint64 valueCentsAt(const AccountHistory& account, const QDate& date);

struct Flows {
    qint64 incomeCents = 0;
    qint64 spendingCents = 0;
    [[nodiscard]] qint64 savingsCents() const { return incomeCents - spendingCents; }
};

// The rows of `accounts` booked from `from` to `to` (both included), without
// the internal transfers.
[[nodiscard]] Flows flows(const QVector<AccountHistory>& accounts, const QDate& from, const QDate& to);

struct Period {
    Flows flows;
    qint64 startValueCents = 0; // at the end of the day before `from`
    qint64 endValueCents = 0;   // at the end of `to`
    [[nodiscard]] qint64 changeCents() const { return endValueCents - startValueCents; }
    [[nodiscard]] qint64 gainsCents() const { return changeCents() - flows.savingsCents(); }
};

[[nodiscard]] Period period(const QVector<AccountHistory>& accounts, const QDate& from, const QDate& to);

} // namespace NetWorthCalc

#endif // FINANCEDASHBOARD_CALC_NETWORTHCALC_H
