#include "AccountController.h"

#include "db/Database.h"
#include "utils/Formatter.h"
#include "models/Account.h"
#include "models/Transaction.h"

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <Qt>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

#include <algorithm>
#include <optional>

namespace {
constexpr int kStatusClearDelayMs = 3000; // how long a status message stays visible
} // namespace

AccountController::AccountController(QObject* parent)
    : QObject(parent)
{
    loadFromDb();
    if (m_accounts.isEmpty() && m_accountCategories.isEmpty() && m_accountTypes.isEmpty()) {
        seedDefaults();
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
    Database& database = Database::instance();

    // Fixed list of banking/financial products, now DB-managed like
    // category (see models/Account.h) — same defaults as the old hardcoded
    // QStringList this replaced.
    static const QVector<QString> kTypeSeed = {
        "CCP", "Livret A", "LDDS", "PEL", "Assurance Vie", "PEA", "CTO", "Other",
    };
    for (const QString& name : kTypeSeed) {
        const int typeId = database.addAccountType(name);
        if (typeId >= 0) {
            m_accountTypes.append(Data::AccountType{ typeId, name });
        }
    }

    // Same 4 usage buckets named in the product request: day-to-day basis,
    // savings, investments, urgent saving.
    struct SeedCat { QString name; QString color; };
    static const QVector<SeedCat> kCatSeed = {
        { "Day-to-day",     "#89b4fa" },
        { "Savings",        "#a6e3a1" },
        { "Investments",    "#cba6f7" },
        { "Urgent savings", "#f9e2af" },
    };
    for (const SeedCat& seedCat : kCatSeed) {
        const int categoryId = database.addAccountCategory(seedCat.name, seedCat.color);
        if (categoryId < 0) {
            continue;
        }
        m_accountCategories.append(Data::AccountCategory{ categoryId, seedCat.name, seedCat.color });
    }

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
    for (const SeedAcc& seedAcc : kAccSeed) {
        const int accountId = database.addAccount(seedAcc.name, seedAcc.type, seedAcc.category, seedAcc.currency, seedAcc.notes);
        if (accountId < 0) {
            continue;
        }

        Data::Account acc;
        acc.id       = accountId;
        acc.name     = seedAcc.name;
        acc.type     = seedAcc.type;
        acc.category = seedAcc.category;
        acc.currency = seedAcc.currency;
        acc.notes    = seedAcc.notes;
        m_accounts.append(acc);
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
            setStatus(QStringLiteral("⚠  An account with this name already exists"));
            return;
        }
    }

    const int accountId = Database::instance().addAccount(trimmed, type, category, currency, notes);
    if (accountId < 0) {
        setStatus(QStringLiteral("⚠  Could not save the account"));
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
    setStatus(QStringLiteral("✓  Account added"));
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
            setStatus(QStringLiteral("⚠  An account with this name already exists"));
            return;
        }
    }

    if (!Database::instance().updateAccount(m_accounts.at(index).id, trimmed, type, category, currency, notes)) {
        setStatus(QStringLiteral("⚠  Could not save the account"));
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
    setStatus(QStringLiteral("✓  Account updated"));
}

void AccountController::removeAccount(int index)
{
    if (index < 0 || index >= m_accounts.size()) {
        return;
    }
    if (!Database::instance().removeAccount(m_accounts.at(index).id)) {
        setStatus(QStringLiteral("⚠  Could not remove the account"));
        return;
    }
    m_accounts.removeAt(index);
    emit accountsChanged();
    setStatus(QStringLiteral("✓  Account removed"));
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
    if (trimmed.isEmpty() || trimmed == m_accountCategories.at(index).name) {
        return;
    }
    if (!Database::instance().renameAccountCategory(m_accountCategories.at(index).id, trimmed)) {
        return;
    }
    // index bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    m_accountCategories[index].name = trimmed; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    emit accountCategoriesChanged();
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
    if (trimmed.isEmpty() || trimmed == m_accountTypes.at(index).name) {
        return;
    }
    if (!Database::instance().renameAccountType(m_accountTypes.at(index).id, trimmed)) {
        return;
    }
    // index bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    m_accountTypes[index].name = trimmed; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    emit accountTypesChanged();
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
        setStatus(QStringLiteral("✓  Hash added"));
    }
    return added;
}

bool AccountController::linkAccountHash(int accountIndex, const QString& hash)
{
    const bool added = addHashInternal(accountIndex, hash, QStringLiteral("auto"));
    if (added) {
        emit accountsChanged();
        setStatus(QStringLiteral("✓  Account linked"));
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
    setStatus(QStringLiteral("✓  Hash removed"));
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
            { "debitText",          transaction.debit  > 0.0 ? Fmt::money(-transaction.debit) : QStringLiteral("—") },
            { "debit",              transaction.debit },
            { "creditText",         transaction.credit > 0.0 ? Fmt::money(transaction.credit, true) : QStringLiteral("—") },
            { "credit",             transaction.credit },
            { "balanceText",        Fmt::money(transaction.balanceAfter) },
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
        setStatus(QStringLiteral("⚠  Could not save the transaction"));
        return false;
    }
    setStatus(QStringLiteral("✓  Transaction updated"));
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
            { "balanceText",  Fmt::money(checkpoint.balance) },
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
        setStatus(QStringLiteral("⚠  Invalid date — use yyyy-MM-dd"));
        return false;
    }
    const int checkpointId = Database::instance().addBalanceCheckpoint(m_accounts.at(accountIndex).id, date, balance, note);
    if (checkpointId < 0) {
        setStatus(QStringLiteral("⚠  Could not save the checkpoint"));
        return false;
    }
    setStatus(QStringLiteral("✓  Balance checkpoint added"));
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
    setStatus(QStringLiteral("✓  Balance checkpoint removed"));
}

QVariantList AccountController::accountBalanceHistory(int accountIndex) const
{
    QVariantList points;
    if (accountIndex < 0 || accountIndex >= m_accounts.size()) {
        return points;
    }
    const int accountId = m_accounts.at(accountIndex).id;

    QVector<Data::Transaction> txs = Database::instance().transactionsForAccount(accountId);
    QVector<Data::Transaction> dated;
    dated.reserve(txs.size());
    for (const Data::Transaction& tx : txs) {
        if (tx.operationDate.isValid()) {
            dated.append(tx);
        }
    }
    if (dated.isEmpty()) {
        return points;
    }
    // Database::transactionsForAccount()/TransactionsTable::allForAccount()
    // orders most-recent-first (for the read-only tables); a running-balance
    // walk needs chronological order instead.
    std::sort(dated.begin(), dated.end(), [](const Data::Transaction& a, const Data::Transaction& b) {
        return a.operationDate < b.operationDate;
    });

    double running = 0.0;
    const std::optional<Data::AccountBalanceCheckpoint> checkpoint =
        Database::instance().latestCheckpointOnOrBefore(accountId, dated.first().operationDate);
    if (checkpoint.has_value()) {
        running = checkpoint->balance;
    } else {
        // No checkpoint at or before the first transaction — infer the
        // balance just before it from its own declared balanceAfter, rather
        // than starting the line at a misleading 0 (product decision).
        const Data::Transaction& first = dated.first();
        running = first.balanceAfter - (first.credit - first.debit);
    }

    auto pushPoint = [&points](const QDate& date, double balance) {
        points.append(QVariantMap{
            { "dateIso",     date.toString(Qt::ISODate) },
            { "dateText",    Fmt::dateOrDash(date) },
            { "timeMs",      static_cast<double>(date.startOfDay().toMSecsSinceEpoch()) },
            { "balance",     balance },
            { "balanceText", Fmt::money(balance) },
        });
    };

    QDate day = dated.first().operationDate;
    for (const Data::Transaction& tx : dated) {
        if (tx.operationDate != day) {
            pushPoint(day, running);
            day = tx.operationDate;
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

    result["currentBalanceText"] = Fmt::money(currentBalance);
    result["currentBalanceRaw"]  = currentBalance;
    result["asOfDateText"]       = asOfText;
    result["transactionCount"]   = Database::instance().transactionCountForAccount(accountId);
    result["hasHistory"]         = !history.isEmpty();
    return result;
}

void AccountController::setStatus(const QString& msg)
{
    m_statusText = msg;
    emit statusChanged();
    QTimer::singleShot(kStatusClearDelayMs, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}
