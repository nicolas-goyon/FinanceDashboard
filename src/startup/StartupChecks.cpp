#include "startup/StartupChecks.h"

#include "db/Database.h"

#include <QDir>
#include <QStandardPaths>
#include <QVariantMap>
#include <QtLogging>

#include <algorithm>

namespace Startup {

QList<StartupStep> StartupChecks::runRequirementChecks()
{
    QList<StartupStep> steps;

    // No hard requirements yet. To add one, append a step here, e.g.:
    //
    //   StartupStep s;
    //   s.name   = QStringLiteral("SQLite driver");
    //   s.fatal  = true;
    //   s.ok     = QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"));
    //   s.detail = s.ok ? QStringLiteral("QSQLITE available")
    //                   : QStringLiteral("QSQLITE driver plugin not found");
    //   s.hint   = QStringLiteral("Re-copy the whole app folder (plugins/sqldrivers).");
    //   steps.append(s);
    //
    // A failed `fatal` step makes main() show StartupErrorWindow.qml instead
    // of the dashboard. See docs/startup.md.

    return steps;
}

QList<StartupStep> StartupChecks::runInitialization(QString& dbPath)
{
    QList<StartupStep> steps;

    // ── Data folder ──────────────────────────────────────────────────────────
    // AppDataLocation resolves to:
    //   Windows : %APPDATA%\DataAnalysis\FinanceDashboard
    //   Linux   : ~/.local/share/DataAnalysis/FinanceDashboard
    //   macOS   : ~/Library/Application Support/DataAnalysis/FinanceDashboard
    const QString dbDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dbPath = dbDir + QDir::separator() + QStringLiteral("finance_dashboard.db");

    StartupStep folder;
    folder.name   = QStringLiteral("Data folder");
    folder.ok     = QDir().mkpath(dbDir);
    folder.detail = dbDir;
    folder.hint   = QStringLiteral("Check that this folder can be created and written to.");
    steps.append(folder);

    // ── Database ─────────────────────────────────────────────────────────────
    // Fatal when this version cannot use the file: a newer version of the
    // app saved it, or the upgrade failed. open() closed the connection, so
    // the dashboard would only show empty pages. Other failures are not
    // fatal, as before the start-up stage existed: the app starts without
    // data.
    StartupStep db;
    db.name   = QStringLiteral("Database");
    db.ok     = Database::instance().open(dbPath);
    db.detail = db.ok ? dbPath
                      : dbPath + QStringLiteral(" — ") + Database::instance().lastError();
    switch (Database::instance().openError()) {
    case Database::OpenError::NewerSchema:
        db.fatal = true;
        db.hint  = QStringLiteral("A newer version of the app saved this file. Install that version or a "
                                  "later one. The file was not changed.");
        break;
    case Database::OpenError::UpgradeFailed:
        db.fatal = true;
        db.hint  = QStringLiteral("The file was not changed. If the disk is full, free some space, then "
                                  "start the app again. If the upgrade fails again, send the application "
                                  "log to the maintainer.");
        break;
    case Database::OpenError::None:
    case Database::OpenError::CannotOpen:
        db.hint  = QStringLiteral("Check the data folder above; delete or restore a corrupted database file.");
        break;
    }
    steps.append(db);

    // ── Database upgrade ─────────────────────────────────────────────────────
    // Only when open() upgraded an existing file: the report tells where the
    // copy from before is. It goes to the log here; Main.qml shows it once.
    const QString upgrade = Database::instance().upgradeReport();
    if (db.ok && !upgrade.isEmpty()) {
        StartupStep step;
        step.name   = QStringLiteral("Database upgrade");
        step.ok     = true;
        step.detail = upgrade;
        steps.append(step);
    }

    return steps;
}

bool StartupChecks::hasFatalFailure(const QList<StartupStep>& steps)
{
    return std::any_of(steps.cbegin(), steps.cend(),
                       [](const StartupStep& s) { return s.fatal && !s.ok; });
}

QVariantList StartupChecks::toVariantList(const QList<StartupStep>& steps)
{
    QVariantList list;
    list.reserve(steps.size());
    for (const StartupStep& s : steps) {
        list.append(QVariantMap{
            {QStringLiteral("name"),   s.name},
            {QStringLiteral("ok"),     s.ok},
            {QStringLiteral("fatal"),  s.fatal},
            {QStringLiteral("detail"), s.detail},
            {QStringLiteral("hint"),   s.hint},
        });
    }
    return list;
}

void StartupChecks::log(const QList<StartupStep>& steps)
{
    for (const StartupStep& s : steps) {
        if (s.ok) {
            qInfo().noquote() << "[startup] OK  " << s.name << "—" << s.detail;
        } else if (s.fatal) {
            qCritical().noquote() << "[startup] FAIL" << s.name << "—" << s.detail << "| fix:" << s.hint;
        } else {
            qWarning().noquote() << "[startup] WARN" << s.name << "—" << s.detail << "| fix:" << s.hint;
        }
    }
}

} // namespace Startup
