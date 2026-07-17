#include "DatabaseManagementController.h"

#include "db/Database.h"
#include "models/Transaction.h"

#include <QDate>
#include <QLatin1String>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

namespace {
constexpr int kStatusClearDelayMs = 4000; // how long a status message stays visible
} // namespace

DatabaseManagementController::DatabaseManagementController(QObject* parent)
    : QObject(parent)
{}

void DatabaseManagementController::resetDatabase()
{
    if (!Database::instance().resetAll()) {
        setStatus(QStringLiteral("⚠  Reset failed — see application log"));
        return;
    }
    setStatus(QStringLiteral("✓  Database reset — everything cleared"));
    emit databaseReset();
}

void DatabaseManagementController::resetToMock()
{
    if (!Database::instance().resetAll()) {
        setStatus(QStringLiteral("⚠  Reset failed — see application log"));
        return;
    }
    seedMockData();
    setStatus(QStringLiteral("✓  Database reset to mock data"));
    emit databaseReset();
}

void DatabaseManagementController::seedMockData() // static, see header
{
    Database& database = Database::instance();

    // Same default category/subcategory list as
    // CategoryController::seedDefaults() (duplicated here deliberately —
    // see class comment).
    struct SeedSub { QString name; QString color; };
    struct SeedCat { QString name; QString color; QVector<SeedSub> subs; };
    static const QVector<SeedCat> kCategorySeed = {
        { "Rent", "#b4befe", {
            { "Base rent", "#b4befe" },
            { "Charges",   "#8c9bfd" },
        } },
        { "Groceries", "#a6e3a1", {
            { "Supermarket", "#a6e3a1" },
            { "Market",      "#88da82" },
        } },
        { "Transport", "#89dceb", {
            { "Public transit", "#89dceb" },
            { "Fuel",           "#66d2e5" },
            { "Rideshare",      "#ace6f1" },
        } },
        { "Utilities", "#f9e2af", {
            { "Electricity", "#f9e2af" },
            { "Internet",    "#f6d489" },
        } },
        { "Leisure", "#cba6f7", {
            { "Restaurants",   "#cba6f7" },
            { "Subscriptions", "#b581f4" },
        } },
        { "Other", "#6c7086", {} },
    };
    for (const SeedCat& seedCat : kCategorySeed) {
        const int catId = database.addCategory(seedCat.name, seedCat.color);
        if (catId < 0) {
            continue;
        }
        for (const SeedSub& seedSub : seedCat.subs) {
            database.addSubcategory(catId, seedSub.name, seedSub.color);
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
        database.addAccountCategory(seedAccCat.name, seedAccCat.color);
    }

    static const QVector<QString> kAccountTypeSeed = {
        "CCP", "Livret A", "LDDS", "PEL", "Assurance Vie", "PEA", "CTO", "Other",
    };
    for (const QString& name : kAccountTypeSeed) {
        database.addAccountType(name);
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
    int checkingBankAId = -1;
    for (const SeedAcc& seedAcc : kAccountSeed) {
        const int accountId = database.addAccount(seedAcc.name, seedAcc.type, seedAcc.category, seedAcc.currency, seedAcc.notes);
        if (seedAcc.name == QLatin1String("Checking — Bank A")) {
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
        static const QVector<SeedTx> kTxSeed = {
            { 28, "Salary",               0.0,    2800.00, 3120.50, "",           ""             },
            { 25, "Monthly rent",         950.00,    0.0,   2170.50, "Rent",       "Base rent"    },
            { 20, "Carrefour",             86.40,    0.0,   2084.10, "Groceries",  "Supermarket"  },
            { 14, "SNCF ticket",           42.00,    0.0,   2042.10, "Transport",  "Public transit" },
            { 9,  "EDF electricity",      118.50,    0.0,   1923.60, "Utilities",  "Electricity"  },
            { 3,  "Cinema + dinner",       64.90,    0.0,   1858.70, "Leisure",    "Restaurants"  },
        };
        QVector<Data::Transaction> txs;
        txs.reserve(kTxSeed.size());
        for (const SeedTx& seedTx : kTxSeed) {
            Data::Transaction transaction;
            transaction.accountId        = checkingBankAId;
            transaction.operationDate     = today.addDays(-seedTx.daysAgo);
            transaction.confirmationDate  = transaction.operationDate;
            transaction.description       = seedTx.description;
            transaction.debit             = seedTx.debit;
            transaction.credit            = seedTx.credit;
            transaction.balanceAfter      = seedTx.balanceAfter;
            transaction.category          = seedTx.category;
            transaction.subcategory       = seedTx.subcategory;
            transaction.sourceTemplate    = QStringLiteral("mock-seed");
            txs.append(transaction);
        }
        database.insertTransactions(checkingBankAId, txs);
    }
}

void DatabaseManagementController::setStatus(const QString& msg)
{
    m_statusText = msg;
    emit statusChanged();
    QTimer::singleShot(kStatusClearDelayMs, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}
