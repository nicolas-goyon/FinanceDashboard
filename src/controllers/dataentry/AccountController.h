#pragma once

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

    QVariantList accounts() const;
    QVariantList accountCategories() const;
    QVariantList accountTypes() const;
    QString      statusText() const   { return m_statusText; }

    // Account CRUD (index into the current accounts() list). Each writes
    // through Database::instance() first; the in-memory list (and
    // accountsChanged()) only updates if that write succeeded. No
    // iban/bic/number params — deliberately not stored (security decision).
    Q_INVOKABLE void addAccount(const QString& name, const QString& type, const QString& category,
                                 const QString& currency, const QString& notes);
    Q_INVOKABLE void updateAccount(int index, const QString& name, const QString& type, const QString& category,
                                    const QString& currency, const QString& notes);
    Q_INVOKABLE void removeAccount(int index);

    // Account category CRUD (index into accountCategories()).
    Q_INVOKABLE void addAccountCategory(const QString& name, const QString& color);
    Q_INVOKABLE void renameAccountCategory(int index, const QString& newName);
    Q_INVOKABLE void setAccountCategoryColor(int index, const QString& color);
    Q_INVOKABLE void removeAccountCategory(int index);
    // Convenience lookup for anywhere that only has the category as a string.
    Q_INVOKABLE QString accountCategoryColor(const QString& categoryName) const;

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
    Q_INVOKABLE void removeHash(int accountIndex, int hashIndex);
    Q_INVOKABLE bool accountHasHash(int accountIndex, const QString& hash) const;

    // Formatted rows for the per-account transactions page
    // (AccountTransactionsPage.qml) — see models/Transaction.h /
    // db/tables/TransactionsTable.h.
    // [{ confirmationDate, operationDate, description, debitText, creditText,
    //    balanceText, category, subcategory }]
    Q_INVOKABLE QVariantList transactionsForAccount(int accountIndex) const;
    Q_INVOKABLE int transactionCountForAccount(int accountIndex) const;

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
    bool addHashInternal(int accountIndex, const QString& hash, const QString& source);

    QVector<Data::Account>         m_accounts;
    QVector<Data::AccountCategory> m_accountCategories;
    QVector<Data::AccountType>     m_accountTypes;
    QString                        m_statusText;
};
