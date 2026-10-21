#ifndef FINANCEDASHBOARD_UTILS_MONEY_H
#define FINANCEDASHBOARD_UTILS_MONEY_H

#include <QtTypes>

#include <algorithm>
#include <cmath>

// ── Money — amounts and whole cents (#33) ────────────────────────────────────
//
// An amount is a double in the models and a REAL in SQLite, in the unit of its
// account (euros or dollars). A double has no exact value for most decimal
// fractions: 0.10 + 0.20 gives 0.30000000000000004. So:
//
//   - the app rounds each amount to the cent before it writes it
//     (TransactionsTable, AccountBalanceCheckpointsTable; schema step 4 rounds
//     the amounts of older databases);
//   - Database refuses an amount that is not a number, or that is larger
//     than kMaxAmount;
//   - a sum or a comparison that must be exact (the reconciliation of an
//     import, a running balance, a total) adds and compares whole cents
//     (toCents(), a qint64), not doubles.
//
// toCents() rounds half away from zero (std::llround), as Fmt::money() and
// the fingerprint of a transaction (TransactionsTable::fingerprint()) do. Up
// to kMaxAmount, toCents(roundToCents(x)) is toCents(x): rounding an amount
// keeps the fingerprint of its row.

namespace Money {

inline constexpr double kCentsPerUnit = 100.0;

// The largest amount that the app stores: 10^13 is 10^15 cents, and up to
// there a double gives back the exact cents of an amount. Fmt::money() can
// show larger amounts (up to 10^15).
inline constexpr double kMaxAmount = 1e13;

// True when the app can store `amount`: a number, not larger than kMaxAmount.
[[nodiscard]] inline bool isValid(double amount)
{
    return std::isfinite(amount) && std::abs(amount) <= kMaxAmount;
}

// `amount` in whole cents. Database refuses the amounts that are not valid,
// so only an older database can still have one: NaN gives 0, and a larger
// amount counts as kMaxAmount.
[[nodiscard]] inline qint64 toCents(double amount)
{
    if (std::isnan(amount)) {
        return 0;
    }
    const double bounded = std::clamp(amount, -kMaxAmount, kMaxAmount);
    return static_cast<qint64>(std::llround(bounded * kCentsPerUnit));
}

[[nodiscard]] inline double fromCents(qint64 cents)
{
    return static_cast<double>(cents) / kCentsPerUnit;
}

// `amount` rounded to the cent. An amount that is not valid stays as it is,
// so that the checks of Database still refuse it.
[[nodiscard]] inline double roundToCents(double amount)
{
    return isValid(amount) ? fromCents(toCents(amount)) : amount;
}

} // namespace Money

#endif // FINANCEDASHBOARD_UTILS_MONEY_H
