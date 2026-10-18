#ifndef FINANCEDASHBOARD_STARTUP_STARTUPCHECKS_H
#define FINANCEDASHBOARD_STARTUP_STARTUPCHECKS_H

#include <QList>
#include <QString>
#include <QVariantList>

// ── StartupChecks ─────────────────────────────────────────────────────────────
//
// The app's start-up stage, run by main() on every launch before the main
// window is loaded: first the *requirement checks* (things that must be true
// for the app to work at all), then the *initialization steps* (preparing the
// data folder, opening the database). Each produces a StartupStep result. If
// any step marked `fatal` fails, main() shows qml/StartupErrorWindow.qml
// (every step listed as met / not met, with a fix hint) instead of Main.qml.
// Checks that need Qt to already be running belong here; anything that can
// stop the app before Qt even loads (missing DLLs, OS too old) is the
// launcher's job (launcher/, see docs/startup.md).
//
// There are currently no hard requirements — runRequirementChecks() is the
// place to add them (see the example comment in StartupChecks.cpp). A
// database that this version cannot use (saved by a newer version, or an
// upgrade that failed) is a fatal step; a database that fails to open for
// another reason is reported but not fatal, as before.

namespace Startup {

struct StartupStep
{
    QString name;     // short label, e.g. "Database"
    bool    ok    = true;
    bool    fatal = false; // true → app can't continue if !ok
    QString detail;   // what was found / done
    QString hint;     // how to fix it (shown only when !ok)
};

class StartupChecks
{
public:
    // Requirements the app needs before anything else runs. Empty for now.
    static QList<StartupStep> runRequirementChecks();

    // Initialization: data folder + database. `dbPath` receives the
    // database file path that was used.
    static QList<StartupStep> runInitialization(QString& dbPath);

    [[nodiscard]] static bool hasFatalFailure(const QList<StartupStep>& steps);

    // For qml/StartupErrorWindow.qml: list of { name, ok, fatal, detail, hint } maps.
    [[nodiscard]] static QVariantList toVariantList(const QList<StartupStep>& steps);

    // Writes every step to the log (qInfo / qWarning / qCritical).
    static void log(const QList<StartupStep>& steps);
};

} // namespace Startup

#endif // FINANCEDASHBOARD_STARTUP_STARTUPCHECKS_H
