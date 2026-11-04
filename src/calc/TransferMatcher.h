#ifndef FINANCEDASHBOARD_CALC_TRANSFERMATCHER_H
#define FINANCEDASHBOARD_CALC_TRANSFERMATCHER_H

#include "models/Transaction.h"

#include <QSet>
#include <QVector>

// ── TransferMatcher ──────────────────────────────────────────────────────────
//
// Finds the internal transfers (#207): a debit on one account of the net
// worth and a credit of the same amount on another account of the net worth,
// a few days apart. No bank identifier is needed: only the accounts, the
// amounts and the dates.
//
// The matcher only suggests. The user confirms or refuses each pair (the
// TransfersController). It looks at the rows that are not decided yet
// (transferId 0). When two pairs share a row, the pair with the smaller gap
// in days wins, then the older debit.
namespace TransferMatcher {

inline constexpr int kDefaultMaxDays = 3;

struct Pair {
    int debitId = -1;
    int creditId = -1;
    int days = 0; // the gap between the two dates
};

// `scope`: the ids of the accounts of the net worth.
[[nodiscard]] QVector<Pair> find(const QVector<Data::Transaction>& rows, const QSet<int>& scope,
                                 int maxDays = kDefaultMaxDays);

} // namespace TransferMatcher

#endif // FINANCEDASHBOARD_CALC_TRANSFERMATCHER_H
