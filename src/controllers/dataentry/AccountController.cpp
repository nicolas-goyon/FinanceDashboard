#include "AccountController.h"

#include "db/Database.h"
#include "utils/Formatter.h"
#include "models/Account.h"
#include "models/Transaction.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <Qt>
#include <QtLogging>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

#include <algorithm>
#include <optional>

namespace {
constexpr int kStatusClearDelayMs = 3000; // how long a status message stays visible
} // namespace

AccountController::AccountController(QObject* parent)
    : QObject(parent)
    , m_status(this, kStatusClearDelayMs, [this]() { emit statusChanged(); })
{
    loadFromDb();
    // Only once per database: empty tables after "Reset Database" are not a
    // fresh DB, and stay empty (#37).
    if (m_accounts.isEmpty() && m_accountCategories.isEmpty() && m_accountTypes.isEmpty()
        && !Database::instance().defaultsWritten(Database::Defaults::Accounts)) {
        seedDefaults();
        loadFromDb(); // all of them, or none
    }
}

void AccountController::loadFromDb()
{
    m_accounts          = Database::instance().loadAccounts();
    m_accountCategories = Database::instance().loadAccountCategories();
    m_accountTypes       = Database::instance().loadAccountTypes();
}

void AccountController::reload()
{
    // Deliberately no reseed-if-empty here — see the header doc comment.
    loadFromDb();
    emit accountsChanged();
    emit accountCategoriesChanged();
    emit accountTypesChanged();
}

void AccountController::seedDefaults()
{
    // Fixed list of banking/financial products, now DB-managed like
    // category (see models/Account.h) — same defaults as the old hardcoded
    // QStringList this replaced.
    static const QVector<QString> kTypeSeed = {
        "CCP", "Livret A", "LDDS", "PEL", "Assurance Vie", "PEA", "CTO", "Other",
    };

    // Same 4 usage buckets named in the product request: day-to-day basis,
    // savings, investments, urgent saving.
    struct SeedCat { QString name; QString color; };
    static const QVector<SeedCat> kCatSeed = {
        { "Day-to-day",     "#89b4fa" },
        { "Savings",        "#a6e3a1" },
        { "Investments",    "#cba6f7" },
        { "Urgent savings", "#f9e2af" },
    };

    // Same accounts as before, minus iban/bic/number (no longer stored — see
    // models/Account.h). Accounts start with zero hashes now (nothing to
    // auto-derive from) — they show up as "not linked" until linked via a
    // statement import or a manually-added hash.
    struct SeedAcc {
        QString name, type, category, currency, notes;
    };
    static const QVector<SeedAcc> kAccSeed = {
        { "Checking — Bank A", "CCP",           "Day-to-day",  "EUR", "" },
        { "Checking — Bank B", "CCP",           "Day-to-day",  "EUR", "" },
        { "Livret A",          "Livret A",      "Savings",     "EUR", "" },
        { "LDDS",              "LDDS",          "Savings",     "EUR", "" },
        { "Assurance Vie",     "Assurance Vie", "Investments", "EUR", "" },
        { "Trading account",  "CTO",           "Investments", "EUR", "" },
    };

    // All the defaults and their record in one SQL transaction: when a write
    // fails, nothing is written, and the next start tries again (#37).
    Database& database = Database::instance();
    const bool written = database.writeDefaults(Database::Defaults::Accounts, [&database]() {
        for (const QString& name : kTypeSeed) {
            if (database.addAccountType(name) < 0) {
                return false;
            }
        }
        for (const SeedCat& seedCat : kCatSeed) {
            if (database.addAccountCategory(seedCat.name, seedCat.color) < 0) {
                return false;
            }
        }
        for (const SeedAcc& seedAcc : kAccSeed) {
            if (database.addAccount(seedAcc.name, seedAcc.type, seedAcc.category, seedAcc.currency, seedAcc.notes) < 0) {
                return false;
            }
        }
        return true;
    });
    if (!written) {
        qWarning() << "[AccountController] could not write the default accounts:" << database.lastError();
    }
}

QVariantList AccountController::accounts() const
{
    QVariantList out;
    out.reserve(m_accounts.size());
    for (const Data::Account& acc : m_accounts) {
        QVariantList hashes;
        hashes.reserve(acc.hashes.size());
        for (const Data::AccountHash& hash : acc.hashes) {
            hashes.append(QVariantMap{
                { "id",     hash.id },
                { "hash",   hash.hash },
                { "source", hash.source },
            });
        }
        out.append(QVariantMap{
            { "id",        acc.id },
            { "name",      acc.name },
            { "type",      acc.type },
            { "category",  acc.category },
            { "currency",  acc.currency },
            { "notes",     acc.notes },
            { "hashes",    hashes },
            { "hashCount", acc.hashes.size() },
            { "linked",    !acc.hashes.isEmpty() },
        });
    }
    return out;
}

QVariantList AccountController::accountCategories() const
{
    QVariantList out;
    out.reserve(m_accountCategories.size());
    for (const Data::AccountCategory& cat : m_accountCategories) {
        out.append(QVariantMap{
            { "name",  cat.name },
            { "color", cat.color },
        });
    }
    return out;
}

QVariantList AccountController::accountTypes() const
{
    QVariantList out;
    out.reserve(m_accountTypes.size());
    for (const Data::AccountType& type : m_accountTypes) {
        out.append(QVariantMap{ { "name", type.name } });
    }
    return out;
}

QString AccountController::accountCategoryColor(const QString& categoryName) const
{
    for (const Data::AccountCategory& cat : m_accountCategories) {
        if (cat.name == categoryName) {
            return cat.color;
        }
    }
    return {};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from AccountsManagePage.qml in this exact order; wrapping
// params in a struct would require rewriting the QML call site too.
void AccountController::addAccount(const QString& name, const QString& type, const QString& category,
                                    const QString& currency, const QString& notes)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    for (const Data::Account& acc : m_accounts) {
        if (acc.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  An account with this name already exists"));
            return;
        }
    }

    const int accountId = Database::instance().addAccount(trimmed, type, category, currency, notes);
    if (accountId < 0) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not save the account"));
        return;
    }

    Data::Account acc;
    acc.id       = accountId;
    acc.name     = trimmed;
    acc.type     = type;
    acc.category = category;
    acc.currency = currency;
    acc.notes    = notes;

    m_accounts.append(acc);
    emit accountsChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Account added"));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from AccountsManagePage.qml in this exact order; wrapping
// params in a struct would require rewriting the QML call site too.
void AccountController::updateAccount(int index, const QString& name, const QString& type, const QString& category,
                                       const QString& currency, const QString& notes)
{
    if (index < 0 || index >= m_accounts.size()) {
        return;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    for (int i = 0; i < m_accounts.size(); ++i) {
        if (i != index && m_accounts.at(i).name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  An account with this name already exists"));
            return;
        }
    }

    if (!Database::instance().updateAccount(m_accounts.at(index).id, trimmed, type, category, currency, notes)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not save the account"));
        return;
    }

    // index is bounds-checked above; QList has no bounds-checked *mutable*
    // accessor (at() is const-only), so operator[] is genuinely required here.
    Data::Account& acc = m_accounts[index]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    acc.name     = trimmed;
    acc.type     = type;
    acc.category = category;
    acc.currency = currency;
    acc.notes    = notes;

    emit accountsChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Account updated"));
}

void AccountController::removeAccount(int index)
{
    if (index < 0 || index >= m_accounts.size()) {
        return;
    }
    if (!Database::instance().removeAccount(m_accounts.at(index).id)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not remove the account"));
        return;
    }
    m_accounts.removeAt(index);
    emit accountsChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Account removed"));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from AccountTypesCategoriesPage.qml.
void AccountController::addAccountCategory(const QString& name, const QString& color)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    for (const Data::AccountCategory& cat : m_accountCategories) {
        if (cat.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return;
        }
    }

    const int categoryId = Database::instance().addAccountCategory(trimmed, color);
    if (categoryId < 0) {
        return;
    }

    m_accountCategories.append(Data::AccountCategory{ categoryId, trimmed, color });
    emit accountCategoriesChanged();
}

void AccountController::renameAccountCategory(int index, const QString& newName)
{
    if (index < 0 || index >= m_accountCategories.size()) {
        return;
    }
    const QString trimmed = newName.trimmed();
    const QString oldName = m_accountCategories.at(index).name;
    if (trimmed.isEmpty() || trimmed == oldName) {
        return;
    }
    // Same check as addAccountCategory() (#34). A change of case only is a rename.
    for (qsizetype i = 0; i < m_accountCategories.size(); ++i) {
        if (i != index && m_accountCategories.at(i).name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  An account category with this name already exists"));
            return;
        }
    }
    // The rename also reaches the accounts in this category.
    if (!Database::instance().renameAccountCategory(m_accountCategories.at(index).id, trimmed)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not rename the account category"));
        return;
    }
    // index bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    m_accountCategories[index].name = trimmed; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (Data::Account& account : m_accounts) {
        if (account.category == oldName) {
            account.category = trimmed;
        }
    }
    emit accountCategoriesChanged();
    emit accountsChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Account category renamed"));
}

void AccountController::setAccountCategoryColor(int index, const QString& color)
{
    if (index < 0 || index >= m_accountCategories.size()) {
        return;
    }
    if (m_accountCategories.at(index).color == color) {
        return;
    }
    if (!Database::instance().setAccountCategoryColor(m_accountCategories.at(index).id, color)) {
        return;
    }
    // index bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    m_accountCategories[index].color = color; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    emit accountCategoriesChanged();
}

void AccountController::removeAccountCategory(int index)
{
    // No guard against in-use accounts still referencing this category name
    // by string — same known-limitation convention as
    // CategoryController::removeCategory() (see ARCHITECTURE.MD TODOs).
    if (index < 0 || index >= m_accountCategories.size()) {
        return;
    }
    if (!Database::instance().removeAccountCategory(m_accountCategories.at(index).id)) {
        return;
    }
    m_accountCategories.removeAt(index);
    emit accountCategoriesChanged();
}

void AccountController::addAccountType(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    for (const Data::AccountType& type : m_accountTypes) {
        if (type.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return;
        }
    }

    const int typeId = Database::instance().addAccountType(trimmed);
    if (typeId < 0) {
        return;
    }

    m_accountTypes.append(Data::AccountType{ typeId, trimmed });
    emit accountTypesChanged();
}

void AccountController::renameAccountType(int index, const QString& newName)
{
    if (index < 0 || index >= m_accountTypes.size()) {
        return;
    }
    const QString trimmed = newName.trimmed();
    const QString oldName = m_accountTypes.at(index).name;
    if (trimmed.isEmpty() || trimmed == oldName) {
        return;
    }
    // Same check as addAccountType() (#34). A change of case only is a rename.
    for (qsizetype i = 0; i < m_accountTypes.size(); ++i) {
        if (i != index && m_accountTypes.at(i).name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  An account type with this name already exists"));
            return;
        }
    }
    // The rename also reaches the accounts of this type.
    if (!Database::instance().renameAccountType(m_accountTypes.at(index).id, trimmed)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not rename the account type"));
        return;
    }
    // index bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    m_accountTypes[index].name = trimmed; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (Data::Account& account : m_accounts) {
        if (account.type == oldName) {
            account.type = trimmed;
        }
    }
    emit accountTypesChanged();
    emit accountsChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Account type renamed"));
}

void AccountController::removeAccountType(int index)
{
    // Same no-guard convention as removeAccountCategory().
    if (index < 0 || index >= m_accountTypes.size()) {
        return;
    }
    if (!Database::instance().removeAccountType(m_accountTypes.at(index).id)) {
        return;
    }
    m_accountTypes.removeAt(index);
    emit accountTypesChanged();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — private helper, both
// call sites below pass a literal source ("manual"/"auto"), not a variable
// that could be swapped with hash.
bool AccountController::addHashInternal(int accountIndex, const QString& hash, const QString& source)
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return false;
    }
    const QString trimmed = hash.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    // accountIndex bounds-checked above; QList's at() is const-only, so
    // mutating through operator[] is genuinely required here.
    Data::Account& acc = m_accounts[accountIndex]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (const Data::AccountHash& existingHash : acc.hashes) {
        if (existingHash.hash.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return false; // already linked
        }
    }

    const int hashId = Database::instance().addAccountHash(acc.id, trimmed, source);
    if (hashId < 0) {
        return false;
    }

    acc.hashes.append(Data::AccountHash{ hashId, trimmed, source });
    return true;
}

bool AccountController::addHash(int accountIndex, const QString& hash)
{
    const bool added = addHashInternal(accountIndex, hash, QStringLiteral("manual"));
    if (added) {
        emit accountsChanged();
        setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Hash added"));
    }
    return added;
}

bool AccountController::linkAccountHash(int accountIndex, const QString& hash)
{
    const bool added = addHashInternal(accountIndex, hash, QStringLiteral("auto"));
    if (added) {
        emit accountsChanged();
        setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Account linked"));
    }
    return added;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from AccountsManagePage.qml as (accountIndex, hashIndex).
void AccountController::removeHash(int accountIndex, int hashIndex)
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return;
    }
    // accountIndex bounds-checked above; QList's at() is const-only, so
    // mutating through operator[] is genuinely required here.
    Data::Account& acc = m_accounts[accountIndex]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (hashIndex < 0 || hashIndex >= acc.hashes.size()) {
        return;
    }
    if (!Database::instance().removeAccountHash(acc.hashes.at(hashIndex).id)) {
        return;
    }
    acc.hashes.removeAt(hashIndex);
    emit accountsChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Hash removed"));
}

bool AccountController::accountHasHash(int accountIndex, const QString& hash) const
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return false;
    }
    const QString trimmed = hash.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    const auto& hashes = m_accounts.at(accountIndex).hashes;
    return std::any_of(hashes.cbegin(), hashes.cend(), [&trimmed](const Data::AccountHash& existingHash) {
        return existingHash.hash.compare(trimmed, Qt::CaseInsensitive) == 0;
    });
}

QVariantList AccountController::transactionsForAccount(int accountIndex) const
{
    QVariantList rows;
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return rows;
    }

    const auto txs = Database::instance().transactionsForAccount(m_accounts.at(accountIndex).id);
    const Fmt::Currency unit = currencyOf(accountIndex);
    rows.reserve(txs.size());
    for (const Data::Transaction& transaction : txs) {
        rows.append(QVariantMap{
            { "id",                  transaction.id },
            { "confirmationDate",    Fmt::dateOrDash(transaction.confirmationDate) },
            { "confirmationDateIso", transaction.confirmationDate.isValid() ? transaction.confirmationDate.toString(Qt::ISODate) : QString() },
            { "operationDate",       Fmt::dateOrDash(transaction.operationDate) },
            { "operationDateIso",    transaction.operationDate.isValid() ? transaction.operationDate.toString(Qt::ISODate) : QString() },
            { "name",                transaction.name },
            { "description",        transaction.description },
            { "debitText",          transaction.debit  > 0.0 ? Fmt::money(-transaction.debit, unit) : QStringLiteral("—") },
            { "debit",              transaction.debit },
            { "creditText",         transaction.credit > 0.0 ? Fmt::money(transaction.credit, unit, true) : QStringLiteral("—") },
            { "credit",             transaction.credit },
            { "balanceText",        Fmt::money(transaction.balanceAfter, unit) },
            { "category",           transaction.category },
            { "subcategory",        transaction.subcategory },
        });
    }
    return rows;
}

int AccountController::transactionCountForAccount(int accountIndex) const
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return 0;
    }
    return Database::instance().transactionCountForAccount(m_accounts.at(accountIndex).id);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from AccountTransactionsPage.qml's edit dialog.
bool AccountController::updateTransaction(int transactionId, const QString& confirmationDateIso, const QString& operationDateIso,
                                           const QString& name, const QString& description, double debit, double credit,
                                           const QString& category, const QString& subcategory)
{
    if (transactionId < 0) {
        return false;
    }

    Data::Transaction tx;
    tx.id               = transactionId;
    tx.confirmationDate = confirmationDateIso.isEmpty() ? QDate() : QDate::fromString(confirmationDateIso, Qt::ISODate);
    tx.operationDate    = operationDateIso.isEmpty()    ? QDate() : QDate::fromString(operationDateIso, Qt::ISODate);
    tx.name             = name;
    tx.description      = description;
    tx.debit            = debit;
    tx.credit           = credit;
    tx.category         = category;
    tx.subcategory      = subcategory;

    if (!Database::instance().updateTransaction(tx)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not save the transaction"));
        return false;
    }
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Transaction updated"));
    return true;
}

QVariantList AccountController::balanceCheckpoints(int accountIndex) const
{
    QVariantList rows;
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return rows;
    }
    const auto checkpoints = Database::instance().loadBalanceCheckpoints(m_accounts.at(accountIndex).id);
    rows.reserve(checkpoints.size());
    for (const Data::AccountBalanceCheckpoint& checkpoint : checkpoints) {
        rows.append(QVariantMap{
            { "id",           checkpoint.id },
            { "date",         checkpoint.date.isValid() ? checkpoint.date.toString(Qt::ISODate) : QString() },
            { "dateText",     Fmt::dateOrDash(checkpoint.date) },
            { "balance",      checkpoint.balance },
            { "balanceText",  Fmt::money(checkpoint.balance, currencyOf(accountIndex)) },
            { "note",         checkpoint.note },
        });
    }
    return rows;
}

bool AccountController::addBalanceCheckpoint(int accountIndex, const QString& dateIso, double balance, const QString& note)
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return false;
    }
    const QDate date = QDate::fromString(dateIso, Qt::ISODate);
    if (!date.isValid()) {
        setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  Invalid date — use yyyy-MM-dd"));
        return false;
    }
    const int checkpointId = Database::instance().addBalanceCheckpoint(m_accounts.at(accountIndex).id, date, balance, note);
    if (checkpointId < 0) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not save the checkpoint"));
        return false;
    }
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Balance checkpoint added"));
    return true;
}

void AccountController::removeBalanceCheckpoint(int accountIndex, int checkpointIndex)
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return;
    }
    const auto checkpoints = Database::instance().loadBalanceCheckpoints(m_accounts.at(accountIndex).id);
    if (checkpointIndex < 0 || checkpointIndex >= checkpoints.size()) {
        return;
    }
    if (!Database::instance().removeBalanceCheckpoint(checkpoints.at(checkpointIndex).id)) {
        return;
    }
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Balance checkpoint removed"));
}

QVariantList AccountController::accountBalanceHistory(int accountIndex) const
{
    QVariantList points;
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return points;
    }
    const int accountId = m_accounts.at(accountIndex).id;

    // The bank's running balance (balanceAfter) follows the booking order:
    // the confirmation date, not the operation date. A card payment made on
    // 02/27 and booked on 03/02 comes after a transfer booked on 03/01. So the
    // replay goes in booking order, and each point is a booking date (#36). A
    // row with no confirmation date uses its operation date
    // (Data::bookingDate()).

    QVector<Data::Transaction> txs = Database::instance().transactionsForAccount(accountId);
    QVector<Data::Transaction> dated;
    dated.reserve(txs.size());
    for (const Data::Transaction& tx : txs) {
        if (Data::bookingDate(tx).isValid()) {
            dated.append(tx);
        }
    }
    if (dated.isEmpty()) {
        return points;
    }
    // Database::transactionsForAccount()/TransactionsTable::allForAccount()
    // orders most-recent-first (for the read-only tables); a running-balance
    // walk needs booking order instead. On one day, the rows keep the order
    // of the statement (the order in which they were saved, so the id).
    std::sort(dated.begin(), dated.end(), [](const Data::Transaction& a, const Data::Transaction& b) {
        const QDate dateA = Data::bookingDate(a);
        const QDate dateB = Data::bookingDate(b);
        return dateA != dateB ? dateA < dateB : a.id < b.id;
    });

    double running = 0.0;
    const std::optional<Data::AccountBalanceCheckpoint> checkpoint =
        Database::instance().latestCheckpointOnOrBefore(accountId, Data::bookingDate(dated.first()));
    if (checkpoint.has_value()) {
        running = checkpoint->balance;
    } else {
        // No checkpoint at or before the first transaction — infer the
        // balance just before it from its own declared balanceAfter, rather
        // than starting the line at a misleading 0 (product decision).
        const Data::Transaction& first = dated.first();
        running = first.balanceAfter - (first.credit - first.debit);
    }

    const Fmt::Currency unit = currencyOf(accountIndex);
    auto pushPoint = [&points, unit](const QDate& date, double balance) {
        points.append(QVariantMap{
            { "dateIso",     date.toString(Qt::ISODate) },
            { "dateText",    Fmt::dateOrDash(date) },
            { "timeMs",      static_cast<double>(date.startOfDay().toMSecsSinceEpoch()) },
            { "balance",     balance },
            { "balanceText", Fmt::money(balance, unit) },
        });
    };

    QDate day = Data::bookingDate(dated.first());
    for (const Data::Transaction& tx : dated) {
        if (Data::bookingDate(tx) != day) {
            pushPoint(day, running);
            day = Data::bookingDate(tx);
        }
        running += (tx.credit - tx.debit);
    }
    pushPoint(day, running); // last day's cumulative balance

    return points;
}

QVariantMap AccountController::accountBalanceSummary(int accountIndex) const
{
    QVariantMap result;
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return result;
    }
    const int accountId = m_accounts.at(accountIndex).id;
    const QVariantList history = accountBalanceHistory(accountIndex);

    double currentBalance = 0.0;
    QString asOfText = QStringLiteral("—");
    if (!history.isEmpty()) {
        const QVariantMap last = history.last().toMap();
        currentBalance = last.value(QStringLiteral("balance")).toDouble();
        asOfText = last.value(QStringLiteral("dateText")).toString();
    } else {
        // No dated transactions at all — fall back to the latest checkpoint
        // on or before today, if any.
        const std::optional<Data::AccountBalanceCheckpoint> checkpoint =
            Database::instance().latestCheckpointOnOrBefore(accountId, QDate::currentDate());
        if (checkpoint.has_value()) {
            currentBalance = checkpoint->balance;
            asOfText = Fmt::dateOrDash(checkpoint->date);
        }
    }

    result["currentBalanceText"] = Fmt::money(currentBalance, currencyOf(accountIndex));
    result["currentBalanceRaw"]  = currentBalance;
    result["currencySymbol"]     = Fmt::currencySymbol(currencyOf(accountIndex));
    result["asOfDateText"]       = asOfText;
    result["transactionCount"]   = Database::instance().transactionCountForAccount(accountId);
    result["hasHistory"]         = !history.isEmpty();
    return result;
}

QVariantMap AccountController::accountCategoryBreakdown(int accountIndex) const
{
    QVariantMap result;
    QVariantList incomeList;
    QVariantList expenseList;
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        result["income"]   = incomeList;
        result["expenses"] = expenseList;
        return result;
    }
    const int accountId = m_accounts.at(accountIndex).id;
    const QVector<Data::Transaction> txs = Database::instance().transactionsForAccount(accountId);

    // Category -> magnitude total; Category -> Subcategory -> magnitude total.
    // Kept as two separate income/expense pairs (rather than one net-total
    // map, like buildImportReport()'s) since a pie slice's value can't be
    // negative — see the header doc comment.
    QMap<QString, double> incomeCategoryTotals, expenseCategoryTotals;
    QMap<QString, QMap<QString, double>> incomeSubTotals, expenseSubTotals;

    for (const Data::Transaction& transaction : txs) {
        const QString category    = transaction.category.isEmpty() ? QStringLiteral("Uncategorized") : transaction.category;
        const QString subcategory = transaction.subcategory.isEmpty() ? QStringLiteral("—") : transaction.subcategory;

        if (transaction.credit > 0.0) {
            incomeCategoryTotals[category] += transaction.credit;
            incomeSubTotals[category][subcategory] += transaction.credit;
        }
        if (transaction.debit > 0.0) {
            expenseCategoryTotals[category] += transaction.debit;
            expenseSubTotals[category][subcategory] += transaction.debit;
        }
    }

    const Fmt::Currency unit = currencyOf(accountIndex);
    auto buildList = [unit](const QMap<QString, double>& categoryTotals,
                             const QMap<QString, QMap<QString, double>>& subTotals) {
        QVariantList list;
        for (auto categoryIt = categoryTotals.constBegin(); categoryIt != categoryTotals.constEnd(); ++categoryIt) {
            QVariantList subcategories;
            const QMap<QString, double> subMap = subTotals.value(categoryIt.key());
            for (auto subIt = subMap.constBegin(); subIt != subMap.constEnd(); ++subIt) {
                subcategories.append(QVariantMap{
                    { "subcategory", subIt.key() },
                    { "totalRaw",    subIt.value() },
                    { "totalText",   Fmt::money(subIt.value(), unit) },
                });
            }
            list.append(QVariantMap{
                { "category",      categoryIt.key() },
                { "totalRaw",      categoryIt.value() },
                { "totalText",     Fmt::money(categoryIt.value(), unit) },
                { "subcategories", subcategories },
            });
        }
        return list;
    };

    result["income"]   = buildList(incomeCategoryTotals, incomeSubTotals);
    result["expenses"] = buildList(expenseCategoryTotals, expenseSubTotals);
    return result;
}

Fmt::Currency AccountController::currencyOf(int accountIndex) const
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return Fmt::currency();
    }
    return Fmt::currencyFromCode(m_accounts.at(accountIndex).currency);
}

void AccountController::setStatus(StatusMessage::Level level, const QString& msg)
{
    m_status.show(level, msg);
}
