#include "db/Database.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("FinanceDashboard");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("DataAnalysis");

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

    return app.exec();
}
