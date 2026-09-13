#include "db/Database.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QString>
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

    // ── Open database ─────────────────────────────────────────────────────────
    // AppDataLocation resolves to:
    //   Windows : %APPDATA%\DataAnalysis\FinanceDashboard\
    //   Linux   : ~/.local/share/DataAnalysis/FinanceDashboard/
    //   macOS   : ~/Library/Application Support/DataAnalysis/FinanceDashboard/
    const QString dbDir  = QStandardPaths::writableLocation(
                               QStandardPaths::AppDataLocation);
    const QString dbPath = dbDir + QDir::separator() + "finance_dashboard.db";

    QDir().mkpath(dbDir);   // create the directory if it doesn't exist yet

    if (!Database::instance().open(dbPath)) {
        qCritical() << "[main] Failed to open database at" << dbPath
                    << "—" << Database::instance().lastError();
        // The app continues — tabs that need the DB will show an error inline.
    } else {
        qInfo() << "[main] Database opened at" << dbPath;
    }

    // ── QML engine ────────────────────────────────────────────────────────────
    QQmlApplicationEngine engine;

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    engine.loadFromModule("FinanceDashboard", "Main");

    return QGuiApplication::exec();
}
