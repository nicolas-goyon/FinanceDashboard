#ifndef FINANCEDASHBOARD_CALC_SAVINGSCALC_H
#define FINANCEDASHBOARD_CALC_SAVINGSCALC_H

#include "models/Transaction.h"

#include <QString>

// ── SavingsCalc ──────────────────────────────────────────────────────────────
//
// Rules of the Savings page (#211), with no database.
namespace SavingsCalc {

// The ceiling of a regulated savings account, from its type, in cents
// (Livret A 22 950 €, LDDS 12 000 €, LEP 10 000 €, PEL 61 200 €, CEL
// 15 300 €, Livret Jeune 1 600 €). 0: no ceiling. The interest can take the
// balance above it.
[[nodiscard]] qint64 ceilingCents(const QString& type);

// A row of interest: a credit whose category, name or description says
// "interest" (or "intérêt"/"interet", as the French banks write it).
[[nodiscard]] bool isInterest(const Data::Transaction& row);

} // namespace SavingsCalc

#endif // FINANCEDASHBOARD_CALC_SAVINGSCALC_H
