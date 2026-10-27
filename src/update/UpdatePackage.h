#ifndef FINANCEDASHBOARD_UPDATE_UPDATEPACKAGE_H
#define FINANCEDASHBOARD_UPDATE_UPDATEPACKAGE_H

#include <QByteArray>
#include <QDir>
#include <QString>

#include <cstdint>

// ── Update::InstallKind and the package of a new version (#117, part 2) ──────
//
// How this copy of the app was installed decides what "Install" does
// (decisions of 2026-10-20): the app runs the setup of the new version, or
// replaces its AppImage; the other copies update by hand. Qt Core only, no
// network: UpdateInstaller downloads the package and its SHA256SUMS.txt.

namespace Update {

// The path of the project on GitHub: its pages and the files of its releases.
inline constexpr const char* kProjectPath = "/nicolas-goyon/FinanceDashboard";

enum class InstallKind : std::uint8_t {
    WindowsSetup,    // installed by the setup: unins000.exe is next to the .exe
    WindowsPortable, // the portable ZIP (or a build directory on Windows)
    AppImage,        // started from an AppImage: $APPIMAGE gives its file
    LinuxPackage,    // the .deb: /opt/FinanceDashboard/bin
    LinuxArchive,    // the .tar.gz (or a build directory on Linux)
};

// `appDir`: the folder of the executable (QCoreApplication::
// applicationDirPath()). `appImage`: $APPIMAGE, which the AppImage runtime
// sets (empty when it is not set). `windows`: the system the app runs on (a
// parameter, so that the tests can try both).
[[nodiscard]] InstallKind installKindOf(const QDir& appDir, const QString& appImage, bool windows);

// True for the copies that the app updates itself: the setup and the AppImage.
[[nodiscard]] bool installsItself(InstallKind kind);

// The name of `kind` for QML: "setup", "portable", "appimage", "deb", "archive".
[[nodiscard]] QString installKindName(InstallKind kind);

// The file of the release that updates `kind`:
// FinanceDashboard-0.2.2-windows-x64.exe, FinanceDashboardTest-0.2.2-exp.57-
// linux-x86_64.AppImage, FinanceDashboard-0.2.2-windows-x64.zip,
// finance-dashboard_0.2.2_amd64.deb, FinanceDashboard-0.2.2-linux-x86_64.tar.gz.
// `version` without the "v" of the tag.
[[nodiscard]] QString packageFileName(InstallKind kind, const QString& appName, const QString& version);

// The SHA-256 of `fileName` in the text of a SHA256SUMS.txt (the format of
// sha256sum: 64 hexadecimal digits, a space, then a space or "*", then the
// file name), in lower case. Empty when the list does not have the file.
[[nodiscard]] QByteArray sha256Of(const QByteArray& sums, const QString& fileName);

} // namespace Update

#endif // FINANCEDASHBOARD_UPDATE_UPDATEPACKAGE_H
