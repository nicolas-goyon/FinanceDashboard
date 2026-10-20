#include "update/UpdatePackage.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QLatin1Char>
#include <QLatin1String>
#include <QList>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>

namespace Update {

InstallKind installKindOf(const QDir& appDir, const QString& appImage, bool windows)
{
    if (windows) {
        // The setup puts its uninstaller next to the .exe; the ZIP has none.
        return QFileInfo::exists(appDir.filePath(QStringLiteral("unins000.exe"))) ? InstallKind::WindowsSetup
                                                                                  : InstallKind::WindowsPortable;
    }
    if (!appImage.isEmpty() && QFileInfo(appImage).isFile()) {
        return InstallKind::AppImage;
    }
    // packaging/linux/make-deb.sh installs the app in /opt/FinanceDashboard.
    if (QDir::cleanPath(appDir.absolutePath()) == QLatin1String("/opt/FinanceDashboard/bin")) {
        return InstallKind::LinuxPackage;
    }
    return InstallKind::LinuxArchive;
}

bool installsItself(InstallKind kind)
{
    return kind == InstallKind::WindowsSetup || kind == InstallKind::AppImage;
}

QString installKindName(InstallKind kind)
{
    switch (kind) {
    case InstallKind::WindowsSetup:    return QStringLiteral("setup");
    case InstallKind::WindowsPortable: return QStringLiteral("portable");
    case InstallKind::AppImage:        return QStringLiteral("appimage");
    case InstallKind::LinuxPackage:    return QStringLiteral("deb");
    case InstallKind::LinuxArchive:    return QStringLiteral("archive");
    }
    return {};
}

QString packageFileName(InstallKind kind, const QString& appName, const QString& version)
{
    // The names of the CI (CPACK_PACKAGE_FILE_NAME, make-appimage.sh,
    // make-deb.sh). The .deb is the prod app only.
    const QString base = appName + QLatin1Char('-') + version;
    switch (kind) {
    case InstallKind::WindowsSetup:    return base + QLatin1String("-windows-x64.exe");
    case InstallKind::WindowsPortable: return base + QLatin1String("-windows-x64.zip");
    case InstallKind::AppImage:        return base + QLatin1String("-linux-x86_64.AppImage");
    case InstallKind::LinuxPackage:    return QLatin1String("finance-dashboard_") + version + QLatin1String("_amd64.deb");
    case InstallKind::LinuxArchive:    return base + QLatin1String("-linux-x86_64.tar.gz");
    }
    return {};
}

QByteArray sha256Of(const QByteArray& sums, const QString& fileName)
{
    static const QRegularExpression pattern(QStringLiteral("^([0-9A-Fa-f]{64}) [ *](.+)$"));
    const QList<QByteArray> lines = sums.split('\n');
    for (const QByteArray& line : lines) {
        QString text = QString::fromUtf8(line);
        if (text.endsWith(QLatin1Char('\r'))) {
            text.chop(1);
        }
        const QRegularExpressionMatch match = pattern.match(text);
        if (match.hasMatch() && match.captured(2) == fileName) {
            return match.captured(1).toLower().toLatin1();
        }
    }
    return {};
}

} // namespace Update
