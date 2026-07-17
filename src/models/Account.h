#ifndef FINANCEDASHBOARD_MODELS_ACCOUNT_H
#define FINANCEDASHBOARD_MODELS_ACCOUNT_H

#include <QString>
#include <QVector>

// ── Data::AccountHash / Data::Account / Data::AccountCategory / Data::AccountType ──
//
// Plain data holders mirroring the row shape of the `accounts` / `account_hashes`
// / `account_categories` / `account_types` DB tables (see db/tables/AccountsTable.h,
// db/tables/AccountHashesTable.h, db/tables/AccountCategoriesTable.h,
// db/tables/AccountTypesTable.h) — same convention as models/Category.h. `id`
// is the SQLite row id (-1 for a not-yet-persisted instance).
//
// Two different classification fields on Account, per product decision (see
// ARCHITECTURE.MD § Account Management), both DB-managed lists edited on the
// "Account types & categories" page:
//   - `type`     — the actual banking/financial product (CCP, Livret A, LDDS,
//                  PEL, Assurance Vie, PEA, CTO, …) — `account_types` table.
//   - `category` — the usage bucket (Day-to-day, Savings, Investments, Urgent
//                  savings, …) — `account_categories` table (also has a
//                  `color`, used for badges; `type` doesn't).
//
// No iban/bic/account-number fields — deliberately removed (see Account
// struct below) for security reasons; accounts are matched to a statement
// import purely via the account_hashes table now.

namespace Data {

struct AccountHash {
    int     id = -1;
    QString hash;
    QString source; // "auto" (derived from IBAN+BIC) or "manual"
};

struct Account {
    int     id = -1;
    QString name;
    QString type;       // e.g. "CCP", "Livret A", "Assurance Vie" — DB-managed list (account_types)
    QString category;   // e.g. "Day-to-day", "Savings" — DB-managed list (account_categories)
    QString currency;   // "EUR" or "USD" — see AccountsManagePage.qml's currency selector
    QString notes;
    QVector<AccountHash> hashes;
    // No iban/bic/number: deliberately NOT stored, for security reasons (see
    // ARCHITECTURE.MD § Account Management) — removed after having been MOCK
    // fields used only to demo hash derivation. Statement import still
    // computes a hash from the *parsed file's* iban/bic (transient, never
    // written to the accounts table) — see AccountHashing.h /
    // SnapshotImportController.
};

struct AccountCategory {
    int     id = -1;
    QString name;
    QString color;
};

struct AccountType {
    int     id = -1;
    QString name;
};

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_ACCOUNT_H
