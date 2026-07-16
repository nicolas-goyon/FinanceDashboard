#pragma once

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
// resetToMock() seeds its own copy of the default categories/account-
// categories/account-types/demo-accounts/sample-transactions rather than
// calling into CategoryController::seedDefaults()/AccountController::
// seedDefaults() (both private, and both also run automatically on first
// launch against an empty DB) — some duplication of the seed *values*, but
// it keeps this controller from needing a back-reference into two unrelated
// singletons just to trigger their private seeding logic.

class DatabaseManagementController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit DatabaseManagementController(QObject* parent = nullptr);

    QString statusText() const { return m_statusText; }

    // Wipes every table to empty. Leaves categories/account-categories/
    // account-types/accounts/transactions all genuinely empty — does NOT
    // reseed (see class comment).
    Q_INVOKABLE void resetDatabase();

    // Wipes every table, then reseeds the same default categories/account-
    // categories/account-types/demo-accounts used on a fresh first launch,
    // plus a handful of fabricated sample transactions against the demo
    // checking accounts so the transactions views have something to show
    // right away.
    Q_INVOKABLE void resetToMock();

signals:
    void statusChanged();
    // Emitted after either action completes successfully. QML listens for
    // this to refresh CategoryController/AccountController (see class
    // comment) — this controller does not call into them itself.
    void databaseReset();

private:
    void setStatus(const QString& msg);
    void seedMockData();

    QString m_statusText;
};
