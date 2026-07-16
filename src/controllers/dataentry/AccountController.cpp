#include "AccountController.h"

#include "db/Database.h"
#include "utils/Formatter.h"

#include <QTimer>
#include <QVariantMap>

AccountController::AccountController(QObject* parent)
    : QObject(parent)
{
    loadFromDb();
    if (m_accounts.isEmpty() && m_accountCategories.isEmpty() && m_accountTypes.isEmpty())
        seedDefaults();
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
    Database& db = Database::instance();

    // Fixed list of banking/financial products, now DB-managed like
    // category (see models/Account.h) — same defaults as the old hardcoded
    // QStringList this replaced.
    static const QVector<QString> kTypeSeed = {
        "CCP", "Livret A", "LDDS", "PEL", "Assurance Vie", "PEA", "CTO", "Other",
    };
    for (const QString& name : kTypeSeed) {
        const int id = db.addAccountType(name);
        if (id >= 0)
            m_accountTypes.append(Data::AccountType{ id, name });
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
    for (const SeedCat& sc : kCatSeed) {
        const int id = db.addAccountCategory(sc.name, sc.color);
        if (id < 0)
            continue;
        m_accountCategories.append(Data::AccountCategory{ id, sc.name, sc.color });
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
    for (const SeedAcc& sa : kAccSeed) {
        const int id = db.addAccount(sa.name, sa.type, sa.category, sa.currency, sa.notes);
        if (id < 0)
            continue;

        Data::Account acc;
        acc.id       = id;
        acc.name     = sa.name;
        acc.type     = sa.type;
        acc.category = sa.category;
        acc.currency = sa.currency;
        acc.notes    = sa.notes;
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
        for (const Data::AccountHash& h : acc.hashes) {
            hashes.append(QVariantMap{
                { "id",     h.id },
                { "hash",   h.hash },
                { "source", h.source },
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
    for (const Data::AccountType& t : m_accountTypes)
        out.append(QVariantMap{ { "name", t.name } });
    return out;
}

QString AccountController::accountCategoryColor(const QString& categoryName) const
{
    for (const Data::AccountCategory& cat : m_accountCategories)
        if (cat.name == categoryName)
            return cat.color;
    return QString();
}

void AccountController::addAccount(const QString& name, const QString& type, const QString& category,
                                    const QString& currency, const QString& notes)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    for (const Data::Account& acc : m_accounts)
        if (acc.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(QStringLiteral("⚠  An account with this name already exists"));
            return;
        }

    const int id = Database::instance().addAccount(trimmed, type, category, currency, notes);
    if (id < 0) {
        setStatus(QStringLiteral("⚠  Could not save the account"));
        return;
    }

    Data::Account acc;
    acc.id       = id;
    acc.name     = trimmed;
    acc.type     = type;
    acc.category = category;
    acc.currency = currency;
    acc.notes    = notes;

    m_accounts.append(acc);
    emit accountsChanged();
    setStatus(QStringLiteral("✓  Account added"));
}

void AccountController::updateAccount(int index, const QString& name, const QString& type, const QString& category,
                                       const QString& currency, const QString& notes)
{
    if (index < 0 || index >= m_accounts.size())
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    for (int i = 0; i < m_accounts.size(); ++i)
        if (i != index && m_accounts[i].name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(QStringLiteral("⚠  An account with this name already exists"));
            return;
        }

    if (!Database::instance().updateAccount(m_accounts[index].id, trimmed, type, category, currency, notes)) {
        setStatus(QStringLiteral("⚠  Could not save the account"));
        return;
    }

    Data::Account& acc = m_accounts[index];
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
    if (index < 0 || index >= m_accounts.size())
        return;
    if (!Database::instance().removeAccount(m_accounts[index].id)) {
        setStatus(QStringLiteral("⚠  Could not remove the account"));
        return;
    }
    m_accounts.removeAt(index);
    emit accountsChanged();
    setStatus(QStringLiteral("✓  Account removed"));
}

void AccountController::addAccountCategory(const QString& name, const QString& color)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    for (const Data::AccountCategory& cat : m_accountCategories)
        if (cat.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return;

    const int id = Database::instance().addAccountCategory(trimmed, color);
    if (id < 0)
        return;

    m_accountCategories.append(Data::AccountCategory{ id, trimmed, color });
    emit accountCategoriesChanged();
}

void AccountController::renameAccountCategory(int index, const QString& newName)
{
    if (index < 0 || index >= m_accountCategories.size())
        return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == m_accountCategories[index].name)
        return;
    if (!Database::instance().renameAccountCategory(m_accountCategories[index].id, trimmed))
        return;
    m_accountCategories[index].name = trimmed;
    emit accountCategoriesChanged();
}

void AccountController::setAccountCategoryColor(int index, const QString& color)
{
    if (index < 0 || index >= m_accountCategories.size())
        return;
    if (m_accountCategories[index].color == color)
        return;
    if (!Database::instance().setAccountCategoryColor(m_accountCategories[index].id, color))
        return;
    m_accountCategories[index].color = color;
    emit accountCategoriesChanged();
}

void AccountController::removeAccountCategory(int index)
{
    // No guard against in-use accounts still referencing this category name
    // by string — same known-limitation convention as
    // CategoryController::removeCategory() (see ARCHITECTURE.MD TODOs).
    if (index < 0 || index >= m_accountCategories.size())
        return;
    if (!Database::instance().removeAccountCategory(m_accountCategories[index].id))
        return;
    m_accountCategories.removeAt(index);
    emit accountCategoriesChanged();
}

void AccountController::addAccountType(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    for (const Data::AccountType& t : m_accountTypes)
        if (t.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return;

    const int id = Database::instance().addAccountType(trimmed);
    if (id < 0)
        return;

    m_accountTypes.append(Data::AccountType{ id, trimmed });
    emit accountTypesChanged();
}

void AccountController::renameAccountType(int index, const QString& newName)
{
    if (index < 0 || index >= m_accountTypes.size())
        return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == m_accountTypes[index].name)
        return;
    if (!Database::instance().renameAccountType(m_accountTypes[index].id, trimmed))
        return;
    m_accountTypes[index].name = trimmed;
    emit accountTypesChanged();
}

void AccountController::removeAccountType(int index)
{
    // Same no-guard convention as removeAccountCategory().
    if (index < 0 || index >= m_accountTypes.size())
        return;
    if (!Database::instance().removeAccountType(m_accountTypes[index].id))
        return;
    m_accountTypes.removeAt(index);
    emit accountTypesChanged();
}

bool AccountController::addHashInternal(int accountIndex, const QString& hash, const QString& source)
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size())
        return false;
    const QString trimmed = hash.trimmed();
    if (trimmed.isEmpty())
        return false;

    Data::Account& acc = m_accounts[accountIndex];
    for (const Data::AccountHash& h : acc.hashes)
        if (h.hash.compare(trimmed, Qt::CaseInsensitive) == 0)
            return false; // already linked

    const int id = Database::instance().addAccountHash(acc.id, trimmed, source);
    if (id < 0)
        return false;

    acc.hashes.append(Data::AccountHash{ id, trimmed, source });
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

void AccountController::removeHash(int accountIndex, int hashIndex)
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size())
        return;
    Data::Account& acc = m_accounts[accountIndex];
    if (hashIndex < 0 || hashIndex >= acc.hashes.size())
        return;
    if (!Database::instance().removeAccountHash(acc.hashes[hashIndex].id))
        return;
    acc.hashes.removeAt(hashIndex);
    emit accountsChanged();
    setStatus(QStringLiteral("✓  Hash removed"));
}

bool AccountController::accountHasHash(int accountIndex, const QString& hash) const
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size())
        return false;
    const QString trimmed = hash.trimmed();
    if (trimmed.isEmpty())
        return false;
    for (const Data::AccountHash& h : m_accounts[accountIndex].hashes)
        if (h.hash.compare(trimmed, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

QVariantList AccountController::transactionsForAccount(int accountIndex) const
{
    QVariantList rows;
    if (accountIndex < 0 || accountIndex >= m_accounts.size())
        return rows;

    const auto txs = Database::instance().transactionsForAccount(m_accounts[accountIndex].id);
    rows.reserve(txs.size());
    for (const Data::Transaction& tx : txs) {
        rows.append(QVariantMap{
            { "confirmationDate", Fmt::dateOrDash(tx.confirmationDate) },
            { "operationDate",    Fmt::dateOrDash(tx.operationDate) },
            { "description",      tx.description },
            { "debitText",        tx.debit  > 0.0 ? Fmt::money(-tx.debit) : QStringLiteral("—") },
            { "creditText",       tx.credit > 0.0 ? Fmt::money(tx.credit, true) : QStringLiteral("—") },
            { "balanceText",      Fmt::money(tx.balanceAfter) },
            { "category",         tx.category },
            { "subcategory",      tx.subcategory },
        });
    }
    return rows;
}

int AccountController::transactionCountForAccount(int accountIndex) const
{
    if (accountIndex < 0 || accountIndex >= m_accounts.size())
        return 0;
    return Database::instance().transactionCountForAccount(m_accounts[accountIndex].id);
}

void AccountController::setStatus(const QString& msg)
{
    m_statusText = msg;
    emit statusChanged();
    QTimer::singleShot(3000, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}
