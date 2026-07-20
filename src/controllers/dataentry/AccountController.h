#ifndef FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_ACCOUNTCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_ACCOUNTCONTROLLER_H

#include "models/Account.h"
#include "models/Transaction.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>
#include <QtQml/qqml.h>

// ── AccountController ─────────────────────────────────────────────────────────
//
// Session-wide source of truth for the accounts list — backs
// qml/pages/AccountsManagePage.qml (add/edit/remove accounts, manage
// per-account hash links, open the transactions page) and
// qml/pages/AccountTypesCategoriesPage.qml (manage the account-type and
// account-category lists), and is read wherever an account gets picked or
// matched: the snapshot table on TransactionsManagementPage.qml and the
// target-account picker on ImportSnapshotPage.qml (see ARCHITECTURE.MD §
// Account Management).
//
// QML_SINGLETON, same reasoning as CategoryController: accounts are
// cross-cutting (edited on one page, read on several others), so a per-page
// instance would silently reset on navigation.
//
// DB-backed (accounts/account_hashes/account_categories/account_types/
// transactions tables).
//
// Three classification/identity notes on Account (see models/Account.h for
// the full rationale):
//   - `type` (CCP, Livret A, Assurance Vie, …) and `category` (Day-to-day,
//     Savings, Investments, Urgent savings, …) are BOTH DB-managed lists now
//     (account_types / account_categories) — `category` also carries a
//     `color` (used for badges), `type` doesn't.
//   - No iban/bic/account-number fields are stored on an account — removed
//     for security reasons (product decision). Matching a statement import
//     to an account is purely via the account_hashes table.
//
// Hashes: an account can carry multiple stored fingerprints (see
// AccountHashesTable). Since accounts no longer store their own iban/bic,
// there is nothing to auto-derive a hash from at account-creation time
// anymore — hashes only get added via linkAccountHash() (from the statement
// import review screen, source "auto") or addHash() (typed in manually on
// AccountsManagePage.qml, source "manual"). `hashCount`/`linked` in
// accounts() let QML show a "not linked yet" badge without walking the
// hashes array itself.
//
// Data shape (rebuilt into a QVariantList on every accounts()/
// accountCategories()/accountTypes() read, same convention as
// CategoryController — no separate QAbstractListModel):
//   accounts: [{ id, name, type, category, currency, notes,
//                hashes: [{ id, hash, source }, ...], hashCount,
//                linked }, ...]
//   accountCategories: [{ name, color }, ...]
//   accountTypes: [{ name }, ...]

class AccountController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(QVariantList accountCategories READ accountCategories NOTIFY accountCategoriesChanged)
    Q_PROPERTY(QVariantList accountTypes READ accountTypes NOTIFY accountTypesChanged)
    Q_PROPERTY(QString      statusText READ statusText NOTIFY statusChanged)

public:
    explicit AccountController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList accounts() const;
    [[nodiscard]] QVariantList accountCategories() const;
    [[nodiscard]] QVariantList accountTypes() const;
    [[nodiscard]] QString      statusText() const   { return m_statusText; }

    // Account CRUD (index into the current accounts() list). Each writes
    // through Database::instance() first; the in-memory list (and
    // accountsChanged()) only updates if that write succeeded. No
    // iban/bic/number params — deliberately not stored (security decision).
    // NOLINTBEGIN(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
    // positionally from AccountsManagePage.qml in this exact order; wrapping
    // params in a struct would require rewriting the QML call sites too.
    Q_INVOKABLE void addAccount(const QString& name, const QString& type, const QString& category,
                                 const QString& currency, const QString& notes);
    Q_INVOKABLE void updateAccount(int index, const QString& name, const QString& type, const QString& category,
                                    const QString& currency, const QString& notes);
    // NOLINTEND(bugprone-easily-swappable-parameters)
    Q_INVOKABLE void removeAccount(int index);

    // Account category CRUD (index into accountCategories()).
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
    // positionally from AccountTypesCategoriesPage.qml.
    Q_INVOKABLE void addAccountCategory(const QString& name, const QString& color);
    Q_INVOKABLE void renameAccountCategory(int index, const QString& newName);
    Q_INVOKABLE void setAccountCategoryColor(int index, const QString& color);
    Q_INVOKABLE void removeAccountCategory(int index);
    // Convenience lookup for anywhere that only has the category as a string.
    Q_INVOKABLE [[nodiscard]] QString accountCategoryColor(const QString& categoryName) const;

    // Account type CRUD (index into accountTypes()). No color — types are
    // plain labels, unlike categories (see models/Account.h).
    Q_INVOKABLE void addAccountType(const QString& name);
    Q_INVOKABLE void renameAccountType(int index, const QString& newName);
    Q_INVOKABLE void removeAccountType(int index);

    // Hash management (index into accounts()). addHash() is for a
    // user-typed hash (source "manual"); linkAccountHash() is for a hash
    // computed from a statement's IBAN/BIC during import review (source
    // "auto") — see ImportSnapshotPage.qml's "Link this account" action.
    // Both no-op (return false) if the hash is empty or already stored for
    // this account.
    Q_INVOKABLE bool addHash(int accountIndex, const QString& hash);
    Q_INVOKABLE bool linkAccountHash(int accountIndex, const QString& hash);
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
    // positionally from AccountsManagePage.qml as (accountIndex, hashIndex).
    Q_INVOKABLE void removeHash(int accountIndex, int hashIndex);
    Q_INVOKABLE [[nodiscard]] bool accountHasHash(int accountIndex, const QString& hash) const;

    // Formatted rows for the per-account transactions page
    // (AccountTransactionsPage.qml) — see models/Transaction.h /
    // db/tables/TransactionsTable.h. Includes the row's real `id` (needed by
    // updateTransaction() below) and the raw ISO dates/amounts alongside the
    // display-formatted text, so the "✎ Edit" dialog can pre-fill its fields
    // without re-parsing Fmt-formatted strings.
    // [{ id, confirmationDate, confirmationDateIso, operationDate, operationDateIso,
    //    name, description, debitText, debit, creditText, credit, balanceText,
    //    category, subcategory }]
    Q_INVOKABLE [[nodiscard]] QVariantList transactionsForAccount(int accountIndex) const;
    Q_INVOKABLE [[nodiscard]] int transactionCountForAccount(int accountIndex) const;

    // Edits an already-persisted transaction (by its real row id, from
    // transactionsForAccount()'s "id" field — NOT an index) — backs
    // AccountTransactionsPage.qml's "✎ Edit" dialog (see ARCHITECTURE.MD §
    // Account Management). Dates are "yyyy-MM-dd" (Qt::ISODate) or empty.
    // balance_after/account_id/source_template are never touched here — see
    // TransactionsTable::updateById()'s doc comment for why (balance
    // continuity is NOT recomputed when debit/credit changes — a known
    // limitation). Returns false if the write failed or the id/dates don't
    // parse; the caller is responsible for re-reading transactionsForAccount()
    // afterward (this method doesn't touch any NOTIFYable property, so QML
    // must re-invoke it explicitly — same reasoning as CategoryController's
    // convenience lookups).
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
    // positionally from AccountTransactionsPage.qml's edit dialog.
    Q_INVOKABLE bool updateTransaction(int transactionId, const QString& confirmationDateIso, const QString& operationDateIso,
                                        const QString& name, const QString& description, double debit, double credit,
                                        const QString& category, const QString& subcategory);

    // ── Balance checkpoints (the "magic number" anchor points — see
    //    ARCHITECTURE.MD § Balance Checkpoints) ──────────────────────────────
    // [{ id, date (yyyy-MM-dd), dateText, balance, balanceText, note }],
    // oldest date first.
    Q_INVOKABLE [[nodiscard]] QVariantList balanceCheckpoints(int accountIndex) const;
    // `dateIso` is "yyyy-MM-dd" (Qt::ISODate) — returns false if it doesn't
    // parse or the account index is invalid.
    Q_INVOKABLE bool addBalanceCheckpoint(int accountIndex, const QString& dateIso, double balance, const QString& note);
    Q_INVOKABLE void removeBalanceCheckpoint(int accountIndex, int checkpointIndex);

    // ── Account History (AccountHistoryPage.qml — read-only, see
    //    ARCHITECTURE.MD § Account History) ─────────────────────────────────
    // Day-by-day running balance, computed by replaying every persisted
    // transaction with a valid operation date in chronological order (one
    // point per calendar day that has at least one transaction, cumulative
    // through that day's last transaction) — the "using each transaction to
    // compute the balance each day" line chart from the product request.
    // Seeded from the latest AccountBalanceCheckpoint on or before the
    // first transaction's date if one exists; otherwise inferred by
    // subtracting the first transaction's own (credit - debit) from its own
    // declared balanceAfter, so the line starts somewhere plausible instead
    // of at a misleading 0 (product decision — see ARCHITECTURE.MD).
    // Returns [] if the account has no dated transactions at all.
    // [{ dateIso, dateText, timeMs, balance, balanceText }, ...] ascending.
    Q_INVOKABLE [[nodiscard]] QVariantList accountBalanceHistory(int accountIndex) const;
    // Current balance = the last point of accountBalanceHistory(), or (if
    // there are no dated transactions at all) the latest balance checkpoint
    // on or before today, or empty/zero if neither exists.
    // { currentBalanceText, currentBalanceRaw, asOfDateText, transactionCount, hasHistory }
    Q_INVOKABLE [[nodiscard]] QVariantMap accountBalanceSummary(int accountIndex) const;

    // Category/subcategory breakdown for the account's two new "Income by
    // category" / "Expenses by category" nested pie charts on
    // AccountHistoryPage.qml. Income/expense is derived per-transaction from
    // credit/debit — NOT a property on Category/CategoryController — a
    // transaction with credit > 0 contributes its credit magnitude to the
    // income side, one with debit > 0 contributes its debit magnitude to the
    // expense side (both, additively, for the rare row that has both). This
    // mirrors SnapshotImportController::buildImportReport()'s byCategory
    // aggregation but split by direction and by magnitude rather than net,
    // since a pie slice needs a positive magnitude, not a signed net that
    // could cancel out. Uses every persisted transaction for the account
    // (same source data as accountBalanceHistory()/transactionsForAccount()),
    // not just dated ones.
    // { income: [{ category, totalRaw, totalText,
    //              subcategories: [{ subcategory, totalRaw, totalText }, ...] }, ...],
    //   expenses: [ ...same shape... ] }
    // Categories/subcategories are alphabetical (QMap's natural key order) —
    // deliberately, so the flattened sub-slice order the QML builds from
    // "subcategories" for a given category always matches the top-level
    // slice order used to build the category ring, keeping the native
    // PieSlice sub-slice rings aligned to their parent (see
    // docs/charting-gotchas.md).
    Q_INVOKABLE [[nodiscard]] QVariantMap accountCategoryBreakdown(int accountIndex) const;

    // Re-reads accounts/account-categories/account-types from Database — no
    // auto-reseed, even if the result comes back completely empty
    // (deliberately: "Reset Database" in Database Management uses this to
    // refresh this singleton after wiping every table, and that action must
    // leave the lists genuinely empty, not silently repopulate them — see
    // ARCHITECTURE.MD § Database Management). "Reset to Mock" reseeds
    // explicitly through Database first, then calls this to pick the fresh
    // rows up. Also called from QML after SnapshotImportController::save()
    // writes to the accounts/hashes tables directly, so this singleton's
    // cached list picks up the change without needing a C++-to-C++
    // back-reference between the two controllers.
    Q_INVOKABLE void reload();

signals:
    void accountsChanged();
    void accountCategoriesChanged();
    void accountTypesChanged();
    void statusChanged();

private:
    void setStatus(const QString& msg);
    void loadFromDb();
    // Seeds the default account types (CCP, Livret A, LDDS, PEL, Assurance
    // Vie, PEA, CTO, Other), the 4 default account categories (Day-to-day,
    // Savings, Investments, Urgent savings), and 6 demo accounts, the first
    // time the app runs against an empty DB (or after a reset — see
    // reload()).
    void seedDefaults();
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — private helper,
    // both call sites (addHash/linkAccountHash below) pass a literal source
    // ("manual"/"auto"), not a variable that could be swapped with hash.
    bool addHashInternal(int accountIndex, const QString& hash, const QString& source);

    QVector<Data::Account>         m_accounts;
    QVector<Data::AccountCategory> m_accountCategories;
    QVector<Data::AccountType>     m_accountTypes;
    QString                        m_statusText;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_ACCOUNTCONTROLLER_H
