#ifndef FINANCEDASHBOARD_CONTROLLERS_DATABASE_DATABASEMANAGEMENTCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_DATABASE_DATABASEMANAGEMENTCONTROLLER_H

#include "utils/StatusMessage.h"

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

// ── DatabaseManagementController ──────────────────────────────────────────────
//
// Backend for qml/pages/DatabaseManagementPage.qml, reached from its own
// sidebar category ("Database Management" — no sub-actions, just this one
// landing page with two buttons). Lets the developer/user wipe the SQLite
// file back to a blank slate, or wipe-then-reseed a small demo dataset —
// useful whenever the schema changes during development, per the product
// request (see ARCHITECTURE.MD § Database Management).
//
// Not a QML_SINGLETON (unlike CategoryController/AccountController) — this
// controller has no state of its own to keep alive across navigation, it
// just triggers an action and reports a status message.
//
// Both actions call Database::instance().resetAll() first (drops and
// recreates every table this app owns, in one transaction — see
// db/Database.h). Neither action reaches into the CategoryController/
// AccountController singletons directly from C++ (same "no C++-to-C++
// singleton coupling" reasoning as SnapshotImportController::save(), see its
// class comment): QML is responsible for calling CategoryController.reload()
// and AccountController.reload() when databaseReset() fires, so those
// singletons' caches pick up the wipe (or the fresh mock data) without a
// stale in-memory list still pointing at now-deleted row ids.
//
// The Test app (#130) also has "Copy my prod database": copyProdDatabase()
// replaces the database of the Test app with a copy of the database of the
// prod app (Database::replaceWithCopyOf()), and emits databaseReset() too.
// prodDatabasePath comes from main.cpp, through Main.qml and the page; it is
// empty in the prod app, and the page then hides the action.
//
// resetToMock() seeds its own copy of the default categories/account-
// categories/account-types/demo-accounts/sample-transactions rather than
// calling into CategoryController::seedDefaults()/AccountController::
// seedDefaults() (both private, and both run once per database, the first
// time that it is used — #37) — some duplication of the seed *values*, but
// it keeps this controller from needing a back-reference into two unrelated
// singletons just to trigger their private seeding logic.

class DatabaseManagementController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    // "success", "warning" or "danger": the StatusBadge variant of statusText (#54).
    Q_PROPERTY(QString statusLevel READ statusLevel NOTIFY statusChanged)

    // ── Copy of the prod database (the Test app only, #130) ──────────────────
    // The database file of the prod app. Empty in the prod app.
    Q_PROPERTY(QString prodDatabasePath READ prodDatabasePath WRITE setProdDatabasePath NOTIFY prodDatabaseChanged)
    // True when that file exists.
    Q_PROPERTY(bool prodDatabaseFound READ prodDatabaseFound NOTIFY prodDatabaseChanged)
    // Where the prod database is and when it changed last, or that it is not there.
    Q_PROPERTY(QString prodDatabaseText READ prodDatabaseText NOTIFY prodDatabaseChanged)
    // What the last copy did, or why it failed (then copyFailed is true).
    // Empty before the first copy.
    Q_PROPERTY(QString copyReport READ copyReport NOTIFY copyReportChanged)
    Q_PROPERTY(bool copyFailed READ copyFailed NOTIFY copyReportChanged)

public:
    explicit DatabaseManagementController(QObject* parent = nullptr);

    [[nodiscard]] QString statusText() const { return m_status.text(); }
    [[nodiscard]] QString statusLevel() const { return m_status.variant(); }

    [[nodiscard]] QString prodDatabasePath() const { return m_prodDatabasePath; }
    void setProdDatabasePath(const QString& path);
    [[nodiscard]] bool prodDatabaseFound() const;
    [[nodiscard]] QString prodDatabaseText() const;
    [[nodiscard]] QString copyReport() const { return m_copyReport; }
    [[nodiscard]] bool copyFailed() const { return m_copyFailed; }

    // Wipes every table to empty. Leaves categories/account-categories/
    // account-types/accounts/transactions all genuinely empty — does NOT
    // reseed (see class comment).
    Q_INVOKABLE void resetDatabase();

    // Wipes every table, then reseeds the same default categories/account-
    // categories/account-types/demo-accounts used on a fresh first launch,
    // plus a handful of fabricated sample transactions on the first demo
    // account ("Checking — Bank A") so the transactions views have something
    // to show right away.
    Q_INVOKABLE void resetToMock();

    // ── Per-account cleanup (see ARCHITECTURE.MD § Database Management) ────────
    // Deletes every transaction for `accountId` but leaves the account, its
    // hashes, and its balance checkpoints in place — narrower than
    // AccountController::removeAccount() (called directly from QML for the
    // "delete the whole account, cascade everything" case; no wrapper needed
    // here since that path already exists and already cascades hashes +
    // transactions + checkpoints, see Database::removeAccount()).
    Q_INVOKABLE bool clearTransactionsForAccount(int accountId);

    // ── Copy of the prod database (the Test app only, #130) ──────────────────
    // Replaces the database of the Test app with a copy of the prod database.
    // After a copy, copyReport tells where the Test data from before is, and
    // what the upgrade of the copy did, if any; databaseReset() is emitted.
    // After a failure, copyReport tells why, and the Test data stays.
    Q_INVOKABLE void copyProdDatabase();
    // The prod app can change its database while the Test app runs: the page
    // calls this when it is shown, so that the date of the last change is
    // current.
    Q_INVOKABLE void refreshProdDatabase();

signals:
    void statusChanged();
    void prodDatabaseChanged();
    void copyReportChanged();
    // Emitted after the reset of either action, also when resetToMock()
    // could not write all the mock data (#38): the pages then show what was
    // written. Also emitted after a copy of the prod database (#130). QML
    // listens for this to refresh CategoryController/
    // AccountController (see class comment) — this controller does not call
    // into them itself.
    void databaseReset();

private:
    void setStatus(StatusMessage::Level level, const QString& msg);
    // False when a write failed (the application log has which one): the
    // status then says it, instead of a success (#38).
    static bool seedMockData();

    StatusMessage m_status;
    QString m_prodDatabasePath;
    QString m_copyReport;
    bool m_copyFailed = false;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_DATABASE_DATABASEMANAGEMENTCONTROLLER_H
