// db_write_test — the writes of the table classes, on a temporary SQLite file.
//
//   #125  a null QString in an optional text column (DEFAULT '') is written
//         as '' instead of failing with "NOT NULL constraint failed"; the
//         Database refuses an empty required value (a name, a currency, an
//         account hash or its source, a checkpoint date) before any write,
//         with a message in lastError()
//   #34   a rename of a category, a subcategory, an account type or an account
//         category reaches every row that uses the name, all or nothing; the
//         controllers refuse a rename to a name that exists (any case)
//   #37   the controllers write their defaults once per database: not again
//         after "Reset Database", across restarts too; all or nothing, and a
//         write that fails is tried again at the next start
//   #38   "Reset to Mock Data" writes the sample transactions
//   #36   the balance history replays the rows in booking order (the
//         confirmation date), the order of the bank's own balance

#include "AccountController.h"
#include "CategoryController.h"
#include "DatabaseManagementController.h"
#include "db/Database.h"
#include "models/Account.h"
#include "models/AssetEntry.h"
#include "models/Category.h"
#include "models/CategoryRule.h"
#include "models/Transaction.h"

#include <QDate>
#include <QScopeGuard>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

#include <algorithm>

namespace {

Data::Transaction transaction(const QString& name, const QString& category, const QString& subcategory)
{
    Data::Transaction tx;
    tx.operationDate    = QDate(2026, 3, 2);
    tx.confirmationDate = QDate(2026, 3, 2);
    tx.name             = name;
    tx.description      = name;
    tx.debit            = 10.0;
    tx.category         = category;
    tx.subcategory      = subcategory;
    tx.sourceTemplate   = QStringLiteral("test");
    return tx;
}

// category/subcategory of each saved transaction, by name.
QHash<QString, QPair<QString, QString>> categoriesByName(int accountId)
{
    QHash<QString, QPair<QString, QString>> result;
    for (const Data::Transaction& tx : Database::instance().transactionsForAccount(accountId)) {
        result.insert(tx.name, {tx.category, tx.subcategory});
    }
    return result;
}

int categoryId(const QString& name)
{
    for (const Data::Category& category : Database::instance().loadCategories()) {
        if (category.name == name) {
            return category.id;
        }
    }
    return -1;
}

int subcategoryId(const QString& category, const QString& name)
{
    for (const Data::Category& c : Database::instance().loadCategories()) {
        if (c.name != category) {
            continue;
        }
        for (const Data::Subcategory& s : c.subcategories) {
            if (s.name == name) {
                return s.id;
            }
        }
    }
    return -1;
}

int indexOfName(const QVariantList& list, const QString& name)
{
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).toMap().value(QStringLiteral("name")).toString() == name) {
            return i;
        }
    }
    return -1;
}

} // namespace

class DbWriteTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void optionalTextCanBeNull();
    void emptyRequiredValuesAreRefused();
    void categoryRenameReachesTransactionsAndRules();
    void subcategoryRenameStaysInItsCategory();
    void accountTypeAndCategoryRenamesReachAccounts();
    void renameIsAllOrNothing();
    void categoryControllerRefusesADuplicateName();
    void accountControllerRefusesADuplicateName();
    void defaultsAreWrittenOncePerDatabase();
    void defaultsAreAllOrNothing();
    void mockDataHasTheSampleTransactions();
    void balanceHistoryFollowsTheBookingOrder();

private:
    QTemporaryDir m_dir;
};

void DbWriteTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QVERIFY2(Database::instance().open(m_dir.filePath(QStringLiteral("test.db"))),
             qPrintable(Database::instance().lastError()));
}

void DbWriteTest::init()
{
    QVERIFY2(Database::instance().resetAll(), qPrintable(Database::instance().lastError()));
}

void DbWriteTest::cleanupTestCase()
{
    // Close the file before QTemporaryDir removes it (Windows keeps an open
    // file).
    Database::instance().close();
}

void DbWriteTest::optionalTextCanBeNull()
{
    Database& db = Database::instance();
    const QString null;
    QVERIFY(null.isNull());

    // AccountsTable: type, category and notes (DEFAULT '').
    const int account = db.addAccount(QStringLiteral("Checking"), null, null, QStringLiteral("EUR"), null);
    QVERIFY2(account >= 0, qPrintable(db.lastError()));
    QVERIFY2(db.updateAccount(account, QStringLiteral("Current"), null, null, QStringLiteral("EUR"), null),
             qPrintable(db.lastError()));
    // AccountBalanceCheckpointsTable: note (DEFAULT '').
    QVERIFY2(db.addBalanceCheckpoint(account, QDate(2026, 3, 1), 100.0, null) >= 0, qPrintable(db.lastError()));

    const QVector<Data::Account> accounts = db.loadAccounts();
    QCOMPARE(accounts.size(), qsizetype(1));
    QCOMPARE(accounts.at(0).name, QStringLiteral("Current"));
    QCOMPARE(accounts.at(0).type, QStringLiteral(""));
    QCOMPARE(accounts.at(0).category, QStringLiteral(""));
    QCOMPARE(accounts.at(0).notes, QStringLiteral(""));
    const QVector<Data::AccountBalanceCheckpoint> checkpoints = db.loadBalanceCheckpoints(account);
    QCOMPARE(checkpoints.size(), qsizetype(1));
    QCOMPARE(checkpoints.at(0).note, QStringLiteral(""));
}

void DbWriteTest::emptyRequiredValuesAreRefused()
{
    Database& db = Database::instance();
    const int account = db.addAccount(QStringLiteral("Checking"), QStringLiteral("CCP"), QStringLiteral(""),
                                      QStringLiteral("EUR"), QStringLiteral(""));
    const int category = db.addCategory(QStringLiteral("Groceries"), QStringLiteral("#a6e3a1"));
    const int subcategory = db.addSubcategory(category, QStringLiteral("Supermarket"), QStringLiteral("#a6e3a1"));
    const int accountCategory = db.addAccountCategory(QStringLiteral("Savings"), QStringLiteral("#89b4fa"));
    const int accountType = db.addAccountType(QStringLiteral("PEA"));
    QVERIFY(account >= 0 && category >= 0 && subcategory >= 0 && accountCategory >= 0 && accountType >= 0);

    const QString emptyName = QStringLiteral("The name is empty.");
    const QString empty = QStringLiteral("");
    // A null name, an empty name and a name of spaces only.
    for (const QString& name : {QString(), empty, QStringLiteral("  ")}) {
        QCOMPARE(db.addCategory(name, QStringLiteral("#a6e3a1")), -1);
        QCOMPARE(db.lastError(), emptyName);
        QVERIFY(!db.renameCategory(category, name));
        QCOMPARE(db.lastError(), emptyName);
        QCOMPARE(db.addSubcategory(category, name, QStringLiteral("#a6e3a1")), -1);
        QCOMPARE(db.lastError(), emptyName);
        QVERIFY(!db.renameSubcategory(subcategory, name));
        QCOMPARE(db.lastError(), emptyName);
        QCOMPARE(db.addAccount(name, QStringLiteral("CCP"), empty, QStringLiteral("EUR"), empty), -1);
        QCOMPARE(db.lastError(), emptyName);
        QVERIFY(!db.updateAccount(account, name, QStringLiteral("CCP"), empty, QStringLiteral("EUR"), empty));
        QCOMPARE(db.lastError(), emptyName);
        QCOMPARE(db.addAccountCategory(name, QStringLiteral("#89b4fa")), -1);
        QCOMPARE(db.lastError(), emptyName);
        QVERIFY(!db.renameAccountCategory(accountCategory, name));
        QCOMPARE(db.lastError(), emptyName);
        QCOMPARE(db.addAccountType(name), -1);
        QCOMPARE(db.lastError(), emptyName);
        QVERIFY(!db.renameAccountType(accountType, name));
        QCOMPARE(db.lastError(), emptyName);
        QCOMPARE(db.addAccountHash(account, name, QStringLiteral("manual")), -1);
        QCOMPARE(db.lastError(), QStringLiteral("The account hash is empty."));
        QCOMPARE(db.addAccountHash(account, QStringLiteral("hash"), name), -1);
        QCOMPARE(db.lastError(), QStringLiteral("The source of the account hash is empty."));
        QCOMPARE(db.addAccount(QStringLiteral("Savings"), QStringLiteral("CCP"), empty, name, empty), -1);
        QCOMPARE(db.lastError(), QStringLiteral("The currency is empty."));
        QVERIFY(!db.updateAccount(account, QStringLiteral("Checking"), QStringLiteral("CCP"), empty, name, empty));
        QCOMPARE(db.lastError(), QStringLiteral("The currency is empty."));
    }
    QCOMPARE(db.addBalanceCheckpoint(account, QDate(), 100.0, empty), -1);
    QCOMPARE(db.lastError(), QStringLiteral("The date of the checkpoint is not valid."));

    // Nothing was written, and no name changed.
    const QVector<Data::Category> categories = db.loadCategories();
    QCOMPARE(categories.size(), qsizetype(1));
    QCOMPARE(categories.at(0).name, QStringLiteral("Groceries"));
    QCOMPARE(categories.at(0).subcategories.size(), qsizetype(1));
    QCOMPARE(categories.at(0).subcategories.at(0).name, QStringLiteral("Supermarket"));
    const QVector<Data::Account> accounts = db.loadAccounts();
    QCOMPARE(accounts.size(), qsizetype(1));
    QCOMPARE(accounts.at(0).name, QStringLiteral("Checking"));
    QVERIFY(accounts.at(0).hashes.isEmpty());
    const QVector<Data::AccountCategory> accountCategories = db.loadAccountCategories();
    QCOMPARE(accountCategories.size(), qsizetype(1));
    QCOMPARE(accountCategories.at(0).name, QStringLiteral("Savings"));
    const QVector<Data::AccountType> accountTypes = db.loadAccountTypes();
    QCOMPARE(accountTypes.size(), qsizetype(1));
    QCOMPARE(accountTypes.at(0).name, QStringLiteral("PEA"));
    QVERIFY(db.loadBalanceCheckpoints(account).isEmpty());
}

void DbWriteTest::categoryRenameReachesTransactionsAndRules()
{
    Database& db = Database::instance();
    const int account = db.addAccount(QStringLiteral("Checking"), QStringLiteral("CCP"), QStringLiteral(""),
                                      QStringLiteral("EUR"), QStringLiteral(""));
    const int groceries = db.addCategory(QStringLiteral("Groceries"), QStringLiteral("#a6e3a1"));
    QVERIFY(db.addSubcategory(groceries, QStringLiteral("Supermarket"), QStringLiteral("#a6e3a1")) >= 0);
    QVERIFY(db.addCategory(QStringLiteral("Leisure"), QStringLiteral("#89b4fa")) >= 0);
    QVERIFY(db.insertTransactions(account, {
        transaction(QStringLiteral("CARREFOUR"), QStringLiteral("Groceries"), QStringLiteral("Supermarket")),
        transaction(QStringLiteral("CINEMA"), QStringLiteral("Leisure"), QStringLiteral("")),
    }));
    QVERIFY(db.addCategoryRule(QStringLiteral("CARREFOUR"), QStringLiteral("Groceries"), QStringLiteral("Supermarket")) >= 0);

    QVERIFY2(db.renameCategory(groceries, QStringLiteral("Food")), qPrintable(db.lastError()));

    const auto rows = categoriesByName(account);
    QCOMPARE(rows.value(QStringLiteral("CARREFOUR")).first, QStringLiteral("Food"));
    QCOMPARE(rows.value(QStringLiteral("CARREFOUR")).second, QStringLiteral("Supermarket"));
    QCOMPARE(rows.value(QStringLiteral("CINEMA")).first, QStringLiteral("Leisure"));
    const QVector<Data::CategoryRule> rules = db.loadCategoryRules();
    QCOMPARE(rules.size(), qsizetype(1));
    QCOMPARE(rules.at(0).category, QStringLiteral("Food"));
    QCOMPARE(categoryId(QStringLiteral("Food")), groceries);
}

void DbWriteTest::subcategoryRenameStaysInItsCategory()
{
    Database& db = Database::instance();
    const int account = db.addAccount(QStringLiteral("Checking"), QStringLiteral("CCP"), QStringLiteral(""),
                                      QStringLiteral("EUR"), QStringLiteral(""));
    const int groceries = db.addCategory(QStringLiteral("Groceries"), QStringLiteral("#a6e3a1"));
    const int leisure = db.addCategory(QStringLiteral("Leisure"), QStringLiteral("#89b4fa"));
    const int sub = db.addSubcategory(groceries, QStringLiteral("Other"), QStringLiteral("#a6e3a1"));
    QVERIFY(db.addSubcategory(leisure, QStringLiteral("Other"), QStringLiteral("#89b4fa")) >= 0);
    QVERIFY(db.insertTransactions(account, {
        transaction(QStringLiteral("MARKET"), QStringLiteral("Groceries"), QStringLiteral("Other")),
        transaction(QStringLiteral("BOWLING"), QStringLiteral("Leisure"), QStringLiteral("Other")),
    }));
    QVERIFY(db.addCategoryRule(QStringLiteral("MARKET"), QStringLiteral("Groceries"), QStringLiteral("Other")) >= 0);
    QVERIFY(db.addCategoryRule(QStringLiteral("BOWLING"), QStringLiteral("Leisure"), QStringLiteral("Other")) >= 0);

    QVERIFY2(db.renameSubcategory(sub, QStringLiteral("Market")), qPrintable(db.lastError()));

    const auto rows = categoriesByName(account);
    QCOMPARE(rows.value(QStringLiteral("MARKET")).second, QStringLiteral("Market"));
    QCOMPARE(rows.value(QStringLiteral("BOWLING")).second, QStringLiteral("Other"));
    for (const Data::CategoryRule& rule : db.loadCategoryRules()) {
        QCOMPARE(rule.subcategory, rule.category == QStringLiteral("Groceries") ? QStringLiteral("Market")
                                                                                : QStringLiteral("Other"));
    }
    QCOMPARE(subcategoryId(QStringLiteral("Groceries"), QStringLiteral("Market")), sub);
}

void DbWriteTest::accountTypeAndCategoryRenamesReachAccounts()
{
    Database& db = Database::instance();
    const int ccp = db.addAccountType(QStringLiteral("CCP"));
    QVERIFY(db.addAccountType(QStringLiteral("PEA")) >= 0);
    const int daily = db.addAccountCategory(QStringLiteral("Day-to-day"), QStringLiteral("#a6e3a1"));
    QVERIFY(db.addAccountCategory(QStringLiteral("Savings"), QStringLiteral("#89b4fa")) >= 0);
    QVERIFY(db.addAccount(QStringLiteral("Checking"), QStringLiteral("CCP"), QStringLiteral("Day-to-day"),
                          QStringLiteral("EUR"), QStringLiteral("")) >= 0);
    QVERIFY(db.addAccount(QStringLiteral("Shares"), QStringLiteral("PEA"), QStringLiteral("Savings"),
                          QStringLiteral("EUR"), QStringLiteral("")) >= 0);

    QVERIFY2(db.renameAccountType(ccp, QStringLiteral("Current account")), qPrintable(db.lastError()));
    QVERIFY2(db.renameAccountCategory(daily, QStringLiteral("Everyday")), qPrintable(db.lastError()));

    for (const Data::Account& account : db.loadAccounts()) {
        if (account.name == QStringLiteral("Checking")) {
            QCOMPARE(account.type, QStringLiteral("Current account"));
            QCOMPARE(account.category, QStringLiteral("Everyday"));
        } else {
            QCOMPARE(account.type, QStringLiteral("PEA"));
            QCOMPARE(account.category, QStringLiteral("Savings"));
        }
    }
}

void DbWriteTest::renameIsAllOrNothing()
{
    Database& db = Database::instance();
    const int account = db.addAccount(QStringLiteral("Checking"), QStringLiteral("CCP"), QStringLiteral(""),
                                      QStringLiteral("EUR"), QStringLiteral(""));
    const int groceries = db.addCategory(QStringLiteral("Groceries"), QStringLiteral("#a6e3a1"));
    QVERIFY(db.insertTransactions(account, {
        transaction(QStringLiteral("CARREFOUR"), QStringLiteral("Groceries"), QStringLiteral("")),
    }));
    // The update of the transactions fails, as a locked file would make it.
    // resetAll() in init() drops the table, and its triggers.
    QSqlQuery query(db.sqlDatabase());
    QVERIFY2(query.exec(QStringLiteral("CREATE TRIGGER fd_test_fail BEFORE UPDATE ON transactions"
                                       " BEGIN SELECT RAISE(ABORT, 'forced by the test'); END;")),
             qPrintable(query.lastError().text()));

    QVERIFY(!db.renameCategory(groceries, QStringLiteral("Food")));
    QCOMPARE(categoryId(QStringLiteral("Groceries")), groceries);   // the category keeps its name
    QCOMPARE(categoryId(QStringLiteral("Food")), -1);
    QCOMPARE(categoriesByName(account).value(QStringLiteral("CARREFOUR")).first, QStringLiteral("Groceries"));
}

void DbWriteTest::categoryControllerRefusesADuplicateName()
{
    Database& db = Database::instance();
    QVERIFY(db.addCategory(QStringLiteral("Groceries"), QStringLiteral("#a6e3a1")) >= 0);
    QVERIFY(db.addCategory(QStringLiteral("Leisure"), QStringLiteral("#89b4fa")) >= 0);
    CategoryController controller;   // not empty: no seed
    const int leisure = indexOfName(controller.categories(), QStringLiteral("Leisure"));
    QVERIFY(leisure >= 0);

    controller.renameCategory(leisure, QStringLiteral("groceries"));
    QCOMPARE(controller.statusLevel(), QStringLiteral("warning"));
    QCOMPARE(categoryId(QStringLiteral("groceries")), -1);
    QVERIFY(categoryId(QStringLiteral("Leisure")) >= 0);

    // A change of case only is a rename.
    controller.renameCategory(leisure, QStringLiteral("LEISURE"));
    QCOMPARE(controller.statusLevel(), QStringLiteral("success"));
    QVERIFY(categoryId(QStringLiteral("LEISURE")) >= 0);
}

void DbWriteTest::accountControllerRefusesADuplicateName()
{
    Database& db = Database::instance();
    QVERIFY(db.addAccountType(QStringLiteral("CCP")) >= 0);
    QVERIFY(db.addAccountType(QStringLiteral("PEA")) >= 0);
    QVERIFY(db.addAccountCategory(QStringLiteral("Savings"), QStringLiteral("#89b4fa")) >= 0);
    QVERIFY(db.addAccountCategory(QStringLiteral("Investments"), QStringLiteral("#cba6f7")) >= 0);
    QVERIFY(db.addAccount(QStringLiteral("Shares"), QStringLiteral("PEA"), QStringLiteral("Investments"),
                          QStringLiteral("EUR"), QStringLiteral("")) >= 0);
    AccountController controller;    // not empty: no seed

    const int pea = indexOfName(controller.accountTypes(), QStringLiteral("PEA"));
    controller.renameAccountType(pea, QStringLiteral("ccp"));
    QCOMPARE(controller.statusLevel(), QStringLiteral("warning"));
    controller.renameAccountType(pea, QStringLiteral("Stocks"));
    QCOMPARE(controller.statusLevel(), QStringLiteral("success"));

    const int investments = indexOfName(controller.accountCategories(), QStringLiteral("Investments"));
    controller.renameAccountCategory(investments, QStringLiteral("SAVINGS"));
    QCOMPARE(controller.statusLevel(), QStringLiteral("warning"));
    controller.renameAccountCategory(investments, QStringLiteral("Markets"));
    QCOMPARE(controller.statusLevel(), QStringLiteral("success"));

    // The controller's own list of accounts follows the rename too.
    const QVariantMap shares = controller.accounts().at(indexOfName(controller.accounts(), QStringLiteral("Shares"))).toMap();
    QCOMPARE(shares.value(QStringLiteral("type")).toString(), QStringLiteral("Stocks"));
    QCOMPARE(shares.value(QStringLiteral("category")).toString(), QStringLiteral("Markets"));
}

void DbWriteTest::defaultsAreWrittenOncePerDatabase()
{
    Database& db = Database::instance();
    db.close();
    // Back to the file of the other tests at the end, also when a check fails.
    const auto reopen = qScopeGuard([this, &db]() {
        db.close();
        db.open(m_dir.filePath(QStringLiteral("test.db")));
    });
    const QString path = m_dir.filePath(QStringLiteral("first-run.db"));
    QVERIFY2(db.open(path), qPrintable(db.lastError()));

    {
        // First run: the defaults are written.
        CategoryController categories;
        AccountController accounts;
        QVERIFY(!categories.categories().isEmpty());
        QVERIFY(!accounts.accounts().isEmpty());
        QVERIFY(!accounts.accountTypes().isEmpty());
    }

    QVERIFY(db.resetAll());
    {
        // The same session after "Reset Database": a controller made now
        // (a page opened after the reset) does not write them again.
        CategoryController categories;
        AccountController accounts;
        QVERIFY(categories.categories().isEmpty());
        QVERIFY(accounts.accounts().isEmpty());
        QVERIFY(accounts.accountTypes().isEmpty());
    }

    // The next start.
    db.close();
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    {
        CategoryController categories;
        AccountController accounts;
        QVERIFY(categories.categories().isEmpty());
        QVERIFY(accounts.accounts().isEmpty());
    }
}

void DbWriteTest::defaultsAreAllOrNothing()
{
    Database& db = Database::instance();
    db.close();
    const auto reopen = qScopeGuard([this, &db]() {
        db.close();
        db.open(m_dir.filePath(QStringLiteral("test.db")));
    });
    QVERIFY2(db.open(m_dir.filePath(QStringLiteral("failed-first-run.db"))), qPrintable(db.lastError()));

    {
        // The account categories are written after the account types. Their
        // write fails, as a full disk would make it.
        QSqlQuery query(db.sqlDatabase());
        QVERIFY2(query.exec(QStringLiteral("CREATE TRIGGER fd_test_fail BEFORE INSERT ON account_categories"
                                           " BEGIN SELECT RAISE(ABORT, 'forced by the test'); END;")),
                 qPrintable(query.lastError().text()));
        AccountController accounts;
        QVERIFY(accounts.accountTypes().isEmpty());
        QVERIFY(db.loadAccountTypes().isEmpty());   // rolled back
        QVERIFY(!db.defaultsWritten(Database::Defaults::Accounts));
        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER fd_test_fail;")));
    }

    // The next start tries again, and writes all of them.
    AccountController accounts;
    QCOMPARE(accounts.accountTypes().size(), qsizetype(8));
    QCOMPARE(accounts.accountCategories().size(), qsizetype(4));
    QCOMPARE(accounts.accounts().size(), qsizetype(6));
    QVERIFY(db.defaultsWritten(Database::Defaults::Accounts));
}

void DbWriteTest::mockDataHasTheSampleTransactions()
{
    DatabaseManagementController controller;
    controller.resetToMock();
    QCOMPARE(controller.statusLevel(), QStringLiteral("success"));
    const QVector<Data::Account> accounts = Database::instance().loadAccounts();
    QCOMPARE(accounts.size(), qsizetype(6));
    QCOMPARE(accounts.at(0).name, QStringLiteral("Checking — Bank A"));
    QCOMPARE(Database::instance().transactionCountForAccount(accounts.at(0).id), 6);
}

void DbWriteTest::balanceHistoryFollowsTheBookingOrder()
{
    // Opening balance 1 000 on 03/01; a transfer of -100 booked on 03/01
    // (bank balance 900); a card payment of -50 made on 02/27 and booked on
    // 03/02 (bank balance 850). Before #36, the replay started from the card
    // row (the oldest operation date): 900, 850, then 750.
    Database& db = Database::instance();
    QVERIFY(db.addAccountType(QStringLiteral("CCP")) >= 0);
    const int withStart = db.addAccount(QStringLiteral("With start"), QStringLiteral("CCP"), QStringLiteral(""),
                                        QStringLiteral("EUR"), QStringLiteral(""));
    const int withoutStart = db.addAccount(QStringLiteral("Without start"), QStringLiteral("CCP"), QStringLiteral(""),
                                           QStringLiteral("EUR"), QStringLiteral(""));
    QVERIFY(db.addBalanceCheckpoint(withStart, QDate(2026, 3, 1), 1000.0, QStringLiteral("Statement start")) >= 0);
    Data::Transaction transfer = transaction(QStringLiteral("TRANSFER"), QStringLiteral(""), QStringLiteral(""));
    transfer.operationDate = QDate(2026, 3, 1);
    transfer.confirmationDate = QDate(2026, 3, 1);
    transfer.debit = 100.0;
    transfer.balanceAfter = 900.0;
    Data::Transaction card = transaction(QStringLiteral("CARD"), QStringLiteral(""), QStringLiteral(""));
    card.operationDate = QDate(2026, 2, 27);
    card.confirmationDate = QDate(2026, 3, 2);
    card.debit = 50.0;
    card.balanceAfter = 850.0;
    QVERIFY(db.insertTransactions(withStart, {transfer, card}));
    QVERIFY(db.insertTransactions(withoutStart, {transfer, card}));

    AccountController controller;
    for (const QString& name : {QStringLiteral("With start"), QStringLiteral("Without start")}) {
        const int index = indexOfName(controller.accounts(), name);
        QVERIFY(index >= 0);
        QCOMPARE(controller.accountBalanceSummary(index).value(QStringLiteral("currentBalanceRaw")).toDouble(), 850.0);
        const QVariantList history = controller.accountBalanceHistory(index);
        QCOMPARE(history.size(), qsizetype(2));
        QCOMPARE(history.at(0).toMap().value(QStringLiteral("dateIso")).toString(), QStringLiteral("2026-03-01"));
        QCOMPARE(history.at(0).toMap().value(QStringLiteral("balance")).toDouble(), 900.0);
        QCOMPARE(history.at(1).toMap().value(QStringLiteral("dateIso")).toString(), QStringLiteral("2026-03-02"));
        QCOMPARE(history.at(1).toMap().value(QStringLiteral("balance")).toDouble(), 850.0);
    }
}

QTEST_GUILESS_MAIN(DbWriteTest)
#include "db_write_test.moc"
