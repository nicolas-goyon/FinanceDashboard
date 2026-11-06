#include "DatabaseManagementController.h"

#include "db/Database.h"
#include "models/DefaultCategories.h"
#include "models/Transaction.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtLogging>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

namespace {
constexpr int kStatusClearDelayMs = 4000; // how long a status message stays visible
} // namespace

DatabaseManagementController::DatabaseManagementController(QObject* parent)
    : QObject(parent)
    , m_status(this, kStatusClearDelayMs, [this]() { emit statusChanged(); })
{}

void DatabaseManagementController::resetDatabase()
{
    if (!Database::instance().resetAll()) {
        // The status says "see application log": the log has the cause (#49).
        qWarning().noquote() << "[DatabaseManagementController] reset failed:" << Database::instance().lastError();
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Reset failed — see application log"));
        return;
    }
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Database reset — everything cleared"));
    emit databaseReset();
}

void DatabaseManagementController::resetToMock()
{
    if (!Database::instance().resetAll()) {
        // The status says "see application log": the log has the cause (#49).
        qWarning().noquote() << "[DatabaseManagementController] reset failed:" << Database::instance().lastError();
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Reset failed — see application log"));
        return;
    }
    if (!seedMockData()) {
        // The reset is done: the pages reload what was written.
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not write all the mock data — see application log"));
        emit databaseReset();
        return;
    }
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Database reset to mock data"));
    emit databaseReset();
}

void DatabaseManagementController::resetCategories()
{
    replaceCategories({}, QStringLiteral("✓  Categories removed"));
}

void DatabaseManagementController::resetCategoriesToDefaults()
{
    replaceCategories(Data::defaultCategories(), QStringLiteral("✓  Categories reset to the defaults"));
}

void DatabaseManagementController::replaceCategories(const QVector<Data::DefaultCategory>& categories,
                                                     const QString& doneText)
{
    Database::CategoryResetStats stats;
    if (!Database::instance().replaceCategories(categories, stats)) {
        qWarning().noquote() << "[DatabaseManagementController] category reset failed:"
                             << Database::instance().lastError();
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Category reset failed — see application log"));
        return;
    }
    QString text = doneText;
    if (stats.uncategorizedTransactions > 0 || stats.removedRules > 0) {
        text += QStringLiteral(" — %1 transaction(s) uncategorized, %2 rule(s) removed")
                    .arg(stats.uncategorizedTransactions)
                    .arg(stats.removedRules);
    }
    setStatus(StatusMessage::Level::Success, text);
    emit databaseReset();
}

bool DatabaseManagementController::clearTransactionsForAccount(int accountId)
{
    if (accountId < 0) {
        return false;
    }
    if (!Database::instance().clearTransactionsForAccount(accountId)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not clear transactions for this account"));
        return false;
    }
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Cleared transactions for this account"));
    return true;
}

void DatabaseManagementController::setProdDatabasePath(const QString& path)
{
    if (path == m_prodDatabasePath) {
        return;
    }
    m_prodDatabasePath = path;
    emit prodDatabaseChanged();
}

bool DatabaseManagementController::prodDatabaseFound() const
{
    return !m_prodDatabasePath.isEmpty() && QFileInfo(m_prodDatabasePath).isFile();
}

QString DatabaseManagementController::prodDatabaseText() const
{
    if (m_prodDatabasePath.isEmpty()) {
        return {};
    }
    const QString path = QDir::toNativeSeparators(m_prodDatabasePath);
    if (!prodDatabaseFound()) {
        return QStringLiteral("There is no prod database at %1. Start Finance Dashboard once, then come back "
                              "to this page.")
            .arg(path);
    }
    // While the prod app runs, its last changes can be in the -wal file only.
    QDateTime changed = QFileInfo(m_prodDatabasePath).lastModified();
    const QFileInfo wal(m_prodDatabasePath + QStringLiteral("-wal"));
    if (wal.exists() && wal.size() > 0 && wal.lastModified() > changed) {
        changed = wal.lastModified();
    }
    return QStringLiteral("Prod database: %1 — last change on %2")
        .arg(path, changed.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
}

void DatabaseManagementController::refreshProdDatabase()
{
    emit prodDatabaseChanged();
}

void DatabaseManagementController::copyProdDatabase()
{
    if (m_prodDatabasePath.isEmpty()) {
        return;
    }
    Database& database = Database::instance();
    QString keptPath;
    m_copyFailed = !database.replaceWithCopyOf(m_prodDatabasePath, keptPath);
    if (m_copyFailed) {
        m_copyReport = database.lastError();
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  The copy failed — see below"));
    } else {
        m_copyReport = QStringLiteral("The Test app now uses a copy of the prod database.\n\n"
                                      "The Test data from before the copy is in:\n%1")
                           .arg(QDir::toNativeSeparators(keptPath));
        const QString upgrade = database.upgradeReport();
        if (!upgrade.isEmpty()) {
            m_copyReport += QStringLiteral("\n\n") + upgrade;
        }
        setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Prod database copied"));
    }
    emit copyReportChanged();
    emit prodDatabaseChanged();
    if (!m_copyFailed) {
        emit databaseReset();
    }
}

bool DatabaseManagementController::seedMockData() // static, see header
{
    Database& database = Database::instance();
    bool written = true;
    // Each write that fails makes the whole seed fail, and is logged.
    const auto check = [&written](bool ok, const char* what) {
        if (!ok) {
            qWarning() << "[DatabaseManagementController] mock data: could not write" << what;
            written = false;
        }
        return ok;
    };

    // The default categories (models/DefaultCategories.h).
    for (const Data::DefaultCategory& seedCat : Data::defaultCategories()) {
        const int catId = database.addCategory(seedCat.name, seedCat.color);
        if (!check(catId >= 0, "a category")) {
            continue;
        }
        for (const Data::DefaultSubcategory& seedSub : seedCat.subcategories) {
            check(database.addSubcategory(catId, seedSub.name, seedSub.color) >= 0, "a subcategory");
        }
    }

    // Same defaults as AccountController::seedDefaults().
    struct SeedAccCat { QString name; QString color; };
    static const QVector<SeedAccCat> kAccountCategorySeed = {
        { "Day-to-day",     "#89b4fa" },
        { "Savings",        "#a6e3a1" },
        { "Investments",    "#cba6f7" },
        { "Urgent savings", "#f9e2af" },
    };
    for (const SeedAccCat& seedAccCat : kAccountCategorySeed) {
        check(database.addAccountCategory(seedAccCat.name, seedAccCat.color) >= 0, "an account category");
    }

    static const QVector<QString> kAccountTypeSeed = {
        "CCP", "Livret A", "LDDS", "PEL", "Assurance Vie", "PEA", "CTO", "Other",
    };
    for (const QString& name : kAccountTypeSeed) {
        check(database.addAccountType(name) >= 0, "an account type");
    }

    struct SeedAcc { QString name, type, category, currency, notes; };
    static const QVector<SeedAcc> kAccountSeed = {
        { "Checking — Bank A", "CCP",           "Day-to-day",  "EUR", "" },
        { "Checking — Bank B", "CCP",           "Day-to-day",  "EUR", "" },
        { "Livret A",          "Livret A",      "Savings",     "EUR", "" },
        { "LDDS",              "LDDS",          "Savings",     "EUR", "" },
        { "Assurance Vie",     "Assurance Vie", "Investments", "EUR", "" },
        { "Trading account",  "CTO",           "Investments", "EUR", "" },
    };
    // The sample transactions go to the first demo account. Found by its
    // place, not by its name: QLatin1String("Checking — Bank A") read the
    // UTF-8 bytes of the em dash as three Latin-1 characters, so the name
    // never matched and no transaction was written (#38).
    int checkingBankAId = -1;
    for (qsizetype i = 0; i < kAccountSeed.size(); ++i) {
        const SeedAcc& seedAcc = kAccountSeed.at(i);
        const int accountId = database.addAccount(seedAcc.name, seedAcc.type, seedAcc.category, seedAcc.currency, seedAcc.notes);
        check(accountId >= 0, "an account");
        if (i == 0) {
            checkingBankAId = accountId;
        }
    }

    // A handful of fabricated sample transactions against the first demo
    // checking account, so the transactions view (Manage Accounts →
    // Transactions) has something real to show right after a mock reset —
    // per product request. Dates are relative to today rather than
    // hardcoded, so they don't look stale months from now.
    if (checkingBankAId >= 0) {
        const QDate today = QDate::currentDate();
        struct SeedTx {
            int daysAgo; QString description; double debit; double credit;
            double balanceAfter; QString category; QString subcategory;
        };
        // The categories of the defaults, in the language of the app.
        const auto cat = [](const char* name) { return QCoreApplication::translate("DefaultCategories", name); };
        const QVector<SeedTx> kTxSeed = {
            { 28, "Salary",          0.0,    2800.00, 3120.50, cat("Income"),    cat("Salary")         },
            { 25, "Monthly rent",    950.00,    0.0,  2170.50, cat("Housing"),   cat("Rent")           },
            { 20, "Carrefour",        86.40,    0.0,  2084.10, cat("Groceries"), cat("Supermarket")    },
            { 14, "SNCF ticket",      42.00,    0.0,  2042.10, cat("Transport"), cat("Public transit") },
            { 9,  "EDF electricity", 118.50,    0.0,  1923.60, cat("Utilities"), cat("Electricity")    },
            { 3,  "Cinema + dinner",  64.90,    0.0,  1858.70, cat("Leisure"),   cat("Restaurants")    },
        };
        QVector<Data::Transaction> txs;
        txs.reserve(kTxSeed.size());
        for (const SeedTx& seedTx : kTxSeed) {
            Data::Transaction transaction;
            transaction.accountId        = checkingBankAId;
            transaction.operationDate     = today.addDays(-seedTx.daysAgo);
            transaction.confirmationDate  = transaction.operationDate;
            // These seed rows are already a short label, not a multi-line
            // "Card payment ...: X / External ref. / Internal ref." style
            // description — name mirrors description here rather than
            // running the real extraction logic (that lives in
            // SumeriaCcpCsvExtractionTemplate, not here).
            transaction.name               = seedTx.description;
            transaction.description       = seedTx.description;
            transaction.debit             = seedTx.debit;
            transaction.credit            = seedTx.credit;
            transaction.balanceAfter      = seedTx.balanceAfter;
            transaction.category          = seedTx.category;
            transaction.subcategory       = seedTx.subcategory;
            transaction.sourceTemplate    = QStringLiteral("mock-seed");
            txs.append(transaction);
        }
        check(database.insertTransactions(checkingBankAId, txs), "the sample transactions");
    }
    return written;
}

void DatabaseManagementController::setStatus(StatusMessage::Level level, const QString& msg)
{
    m_status.show(level, msg);
}
