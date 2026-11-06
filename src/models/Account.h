#ifndef FINANCEDASHBOARD_MODELS_ACCOUNT_H
#define FINANCEDASHBOARD_MODELS_ACCOUNT_H

#include <QCoreApplication>
#include <QString>
#include <QStringView>
#include <QDate>
#include <QVector>

// ── Data::AccountHash / Data::Account / Data::AccountCategory / Data::AccountType /
//    Data::AccountBalanceCheckpoint ──────────────────────────────────────────
//
// Plain data holders mirroring the row shape of the `accounts` / `account_hashes`
// / `account_categories` / `account_types` / `account_balance_checkpoints` DB
// tables (see db/tables/AccountsTable.h, db/tables/AccountHashesTable.h,
// db/tables/AccountCategoriesTable.h, db/tables/AccountTypesTable.h,
// db/tables/AccountBalanceCheckpointsTable.h) — same convention as
// models/Category.h. `id` is the SQLite row id (-1 for a not-yet-persisted
// instance).
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
// struct below): the app does not keep them. Accounts are matched to a
// statement import purely via the account_hashes table now (a fingerprint,
// not a security feature: see utils/AccountHashing.h).

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
    // Part of the net worth (#206): the accounts of the user (checking,
    // savings, investments). A move between two of them is neither income
    // nor spending. Off for an account that the user only follows (a joint
    // account, the account of a child): money that goes to it is spending.
    bool    inNetWorth = true;
    QVector<AccountHash> hashes;
    // No iban/bic/number: deliberately NOT stored, the app does not keep them
    // (see ARCHITECTURE.MD § Account Management) — removed after having been MOCK
    // fields used only to demo hash derivation. Statement import still
    // computes a hash from the *parsed file's* iban/bic (transient, never
    // written to the accounts table) — see AccountHashing.h /
    // SnapshotImportController.
};

// An investment account (#208): its value changes with the market, so its
// balance checkpoints are valuations that the user enters from the statement
// of the bank. The account category "Investments", or a type of the default
// list for investments. The category name in English or French.
// A default account category, in the language of the app (context
// "DefaultAccountCategories"). The seeders write its name; the name is data
// after that.
[[nodiscard]] inline QString accountCategory(const char* englishName)
{
    return QCoreApplication::translate("DefaultAccountCategories", englishName);
}

// True when `category` is the default account category `englishName` in one
// of the languages of the app: a database made in French has "\u00c9pargne",
// not "Savings". The French names are written here too, so that a database
// made in French still works after a switch to English.
[[nodiscard]] inline bool isDefaultAccountCategory(const QString& category, const char* englishName,
                                                   QStringView frenchName)
{
    return category.compare(QLatin1String(englishName), Qt::CaseInsensitive) == 0
           || category.compare(frenchName, Qt::CaseInsensitive) == 0
           || category.compare(accountCategory(englishName), Qt::CaseInsensitive) == 0;
}

[[nodiscard]] inline bool isInvestment(const Account& account)
{
    if (isDefaultAccountCategory(account.category, "Investments", u"Investissements")) {
        return true;
    }
    for (const char* type : {"PEA", "CTO", "Assurance Vie"}) {
        if (account.type.compare(QLatin1String(type), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

// A savings account (#209, #211): the account category "Savings", or a type
// of the default list for regulated savings. An investment account is not a
// savings account.
[[nodiscard]] inline bool isSavings(const Account& account)
{
    if (isInvestment(account)) {
        return false;
    }
    if (isDefaultAccountCategory(account.category, "Savings", u"\u00c9pargne")) {
        return true;
    }
    for (const char* type : {"Livret A", "LDDS", "LEP", "PEL", "CEL", "Livret Jeune"}) {
        if (account.type.compare(QLatin1String(type), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

struct AccountCategory {
    int     id = -1;
    QString name;
    QString color;
};

struct AccountType {
    int     id = -1;
    QString name;
};

// A user-entered "the balance of this account was X on this date" anchor
// point (the "magic number" from the product request) — lets reconciliation
// (see SnapshotImportController::checkReconciliation()) validate an
// imported statement's declared opening balance against something real even
// when full transaction history since account opening hasn't been imported.
// Mirrors the `account_balance_checkpoints` table (see
// db/tables/AccountBalanceCheckpointsTable.h). An account can carry several
// checkpoints over time (e.g. one per year that can't be fully imported).
struct AccountBalanceCheckpoint {
    int     id = -1;
    int     accountId = -1;
    QDate   date;
    double  balance = 0.0;
    QString note;
};

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_ACCOUNT_H
