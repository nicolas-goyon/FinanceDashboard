#ifndef FINANCEDASHBOARD_CONTROLLERS_UPDATES_UPDATEINSTALLER_H
#define FINANCEDASHBOARD_CONTROLLERS_UPDATES_UPDATEINSTALLER_H

#include "update/ReleaseFeed.h"
#include "update/UpdatePackage.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtTypes>

#include <cstdint>
#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

// ── UpdateInstaller ──────────────────────────────────────────────────────────
//
// The second part of #117: the download and the installation of the newest
// version of the channel, for UpdateController (decisions of 2026-10-20).
//
//   1. start(), on a click on "Install": downloads SHA256SUMS.txt and the
//      package of the release (the setup, or the AppImage), and compares the
//      SHA-256 of the package with the list. Nothing is downloaded before.
//   2. The setup: the file waits in downloadDir() (state Ready) until the
//      user clicks "Install and restart". The AppImage: the new file replaces
//      the AppImage of the app at once (state Installed); the app runs on.
//   3. restart(), on that click: runs the setup silently (Installing): it
//      closes the app (the Restart Manager of Windows), installs, and starts
//      the new version (packaging/windows/update-restart.iss). Or starts the
//      new AppImage, and the app quits.
//
// The downloads use the network access of UpdateController: no cookie, the
// User-Agent of the app, "Accept-Language: en". They read only the files of
// the releases of the project on GitHub
// (https://github.com/nicolas-goyon/FinanceDashboard/releases/download/...),
// and follow redirects only to GitHub (github.com, *.githubusercontent.com).
// The tests use a local server: when the list of releases is on an address of
// this computer (127.0.0.1), its origin is allowed too (setAllowedOrigin());
// the list of GitHub is not, so this rule never applies to the app.
//
// The setup is checked again (SHA-256) just before it runs: it waited in a
// folder that the user can write. A second setup stops at once while the
// first one runs (SetupMutex, CMakeLists.txt).

class UpdateInstaller : public QObject
{
    Q_OBJECT

public:
    enum class State : std::uint8_t {
        Idle,
        Downloading,
        Ready,      // the setup is downloaded and checked: "Install and restart"
        Installed,  // the AppImage is replaced: "Restart now"
        Installing, // the setup runs: it closes the app and starts it again
        Failed,
    };

    explicit UpdateInstaller(QNetworkAccessManager* network, QObject* parent = nullptr);
    ~UpdateInstaller() override;
    UpdateInstaller(const UpdateInstaller&) = delete;
    UpdateInstaller& operator=(const UpdateInstaller&) = delete;
    UpdateInstaller(UpdateInstaller&&) = delete;
    UpdateInstaller& operator=(UpdateInstaller&&) = delete;

    static constexpr int kTimeoutMs = 60000;   // a download that gets no data for 60 s fails
    static constexpr int kInstallWaitMs = 60000; // then "Install and restart" again
    static constexpr const char* kSumsFileName = "SHA256SUMS.txt";
    static constexpr const char* kSetupLogName = "setup.log"; // in downloadDir()

    void setKind(Update::InstallKind kind) { m_kind = kind; }
    [[nodiscard]] Update::InstallKind kind() const { return m_kind; }
    // The AppImage kind: the file that the new AppImage replaces ($APPIMAGE).
    void setAppImagePath(const QString& path) { m_appImagePath = path; }
    // The setup kind: the folder of the downloaded setup and of its log.
    void setDownloadDir(const QString& dir) { m_downloadDir = dir; }
    [[nodiscard]] QString downloadDir() const { return m_downloadDir; }
    // The origin (scheme, host, port) of the list of releases: allowed too,
    // when it is an address of this computer (the tests).
    void setAllowedOrigin(const QUrl& url) { m_allowedOrigin = url; }
    void setUserAgent(const QByteArray& userAgent) { m_userAgent = userAgent; }

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString stateName() const;
    [[nodiscard]] double progress() const { return m_progress; }
    [[nodiscard]] QString message() const { return m_message; }
    // The version that start() downloads or installed.
    [[nodiscard]] QString version() const { return m_version; }

    // Downloads `packageName` and SHA256SUMS.txt of `release` (the version
    // `version`), then checks and keeps (setup) or applies (AppImage) the
    // package. Only in the states Idle and Failed.
    void start(const Update::Release& release, const QString& packageName, const QString& version);
    // Stops a download, and back to Idle. A Failed state also goes back to Idle.
    void cancel();
    // Ready: runs the setup. Installed: starts the new AppImage and quits.
    void restart();

    // The command line of the setup (Inno Setup): silent, the Restart
    // Manager closes the app, and /RESTARTAPP=1 makes the setup start the new
    // version (packaging/windows/update-restart.iss). The CI test of the
    // update (.github/workflows/ci.yml) uses the same arguments.
    [[nodiscard]] static QStringList setupArguments();
    // True for a file of a release of the project on GitHub.
    [[nodiscard]] static bool isProjectDownload(const QUrl& url);
    // True for an address of GitHub that a download can be redirected to.
    [[nodiscard]] static bool isGitHubDownloadHost(const QUrl& url);
    // `environment` without what the AppRun of the old AppImage added (its
    // folder in LD_LIBRARY_PATH, QT_PLUGIN_PATH, ...): the environment of the
    // new AppImage.
    [[nodiscard]] static QProcessEnvironment environmentForNewAppImage(const QProcessEnvironment& environment);
    // Removes the files of an earlier download from `dir`, but not the log of
    // the setup when `keepLog` (at start: the log of a failed update stays
    // until the next download). Errors do not matter: a file in use stays
    // until the next time.
    static void removeOldDownloads(const QString& dir, bool keepLog);

    // The tests only: in place of QProcess::startDetached() and of
    // QCoreApplication::quit().
    using Launcher = std::function<bool(const QString& program, const QStringList& arguments,
                                        const QProcessEnvironment& environment)>;
    void setLauncherForTests(Launcher launcher) { m_launcher = std::move(launcher); }
    void setQuitForTests(std::function<void()> quit) { m_quit = std::move(quit); }
    void setTimeoutForTests(int milliseconds) { m_timeoutMs = milliseconds; }
    void setInstallWaitForTests(int milliseconds) { m_installWait.setInterval(milliseconds); }

signals:
    void changed();

private:
    [[nodiscard]] bool allowed(const QUrl& url) const;
    [[nodiscard]] bool allowedRedirect(const QUrl& url) const;
    QNetworkReply* get(const QUrl& url);
    [[nodiscard]] QString downloadError(QNetworkReply* reply, const QString& fileName) const;
    void sumsFinished(QNetworkReply* reply);
    void packageData(QNetworkReply* reply);
    void packageFinished(QNetworkReply* reply);
    void apply();
    [[nodiscard]] bool setupUnchanged() const;
    void fail(const QString& message);
    void setState(State state, const QString& message);
    void stopReply();

    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
    Update::InstallKind m_kind = Update::InstallKind::LinuxArchive;
    QString m_appImagePath;
    QString m_downloadDir;
    QUrl m_allowedOrigin;
    QByteArray m_userAgent;
    State m_state = State::Idle;
    double m_progress = 0.0;
    QString m_message;
    QString m_version;
    QString m_packageName;
    QUrl m_packageUrl;
    qint64 m_packageSize = 0;     // the size of the asset in the list of releases
    QByteArray m_expectedSha256;  // from SHA256SUMS.txt, lower case hexadecimal
    std::unique_ptr<QSaveFile> m_file; // the download: in its place after the check only
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    qint64 m_received = 0;
    int m_lastPercent = -1;
    int m_timeoutMs = kTimeoutMs;
    QString m_setupPath;          // the checked setup (state Ready)
    QTimer m_installWait;
    Launcher m_launcher;
    std::function<void()> m_quit;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_UPDATES_UPDATEINSTALLER_H
