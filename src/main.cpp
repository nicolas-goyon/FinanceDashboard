#include "db/Database.h"
#include "startup/StartupChecks.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QList>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QString>
#include <QVariantMap>
#include <Qt>
#include <QtLogging>

// argv's C-array signature is mandated by the C++ standard for main() — it
// cannot be a std::array (size isn't known at compile time) or std::vector
// (main() is constructed by the runtime, not by our code).
// The "no header providing main" warning below is a known clang-tidy/Qt
// false positive (Qt's Windows entry-point shim redefines `main` via a
// macro), not a real missing include — there is no header for it to add.
int main(int argc, char* argv[]) // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,misc-include-cleaner)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("FinanceDashboard");
    // The version string comes from version.json, via the FD_VERSION_STRING
    // compile definition set in CMakeLists.txt — see CI-CD.md. Never hard-code
    // it here: this is the number the About/window title and any future update
    // check would report, and it has to match the installer and the release tag.
    QGuiApplication::setApplicationVersion(QStringLiteral(FD_VERSION_STRING));
    QGuiApplication::setOrganizationName("DataAnalysis");

    // Must be set before any QML importing Qt Quick Controls is loaded
    // (QQuickStyle docs: "before loading QML that imports Qt Quick Controls").
    // The platform-native styles (Windows, macOS, ...) refuse `background:` /
    // `contentItem:` customization on Controls (TextField, ComboBox, Popup, ...),
    // which this app relies on everywhere — hence "Basic", the style Qt docs
    // recommend for full customization. See:
    // https://doc.qt.io/qt-6/qtquickcontrols2-customize.html#customization-reference
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // ── Start-up stage ────────────────────────────────────────────────────────
    // Requirement checks, then initialization (data folder + database), on
    // every launch — see src/startup/StartupChecks.h and docs/startup.md.
    // A failed *fatal* step shows StartupErrorWindow.qml (every step listed as
    // met / not met) instead of the dashboard. (Failures that stop the app
    // before this code can run — missing DLLs, OS too old — are reported by
    // the launcher, launcher/.)
    QString dbPath;
    QList<Startup::StartupStep> steps = Startup::StartupChecks::runRequirementChecks();
    if (!Startup::StartupChecks::hasFatalFailure(steps)) {
        steps += Startup::StartupChecks::runInitialization(dbPath);
    }
    Startup::StartupChecks::log(steps);

    // ── QML engine ────────────────────────────────────────────────────────────
    QQmlApplicationEngine engine;

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    if (Startup::StartupChecks::hasFatalFailure(steps)) {
        engine.setInitialProperties(
            {{QStringLiteral("steps"), Startup::StartupChecks::toVariantList(steps)}});
        engine.loadFromModule("FinanceDashboard", "StartupErrorWindow");
    } else {
        // Main.qml shows it once when this start upgraded the database.
        engine.setInitialProperties(
            {{QStringLiteral("databaseNotice"), Database::instance().upgradeReport()}});
        engine.loadFromModule("FinanceDashboard", "Main");
    }

    return QGuiApplication::exec();
}
