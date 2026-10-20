#include "UpdateInstaller.h"

#include "update/ReleaseFeed.h"
#include "update/UpdatePackage.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QLatin1Char>
#include <QLatin1String>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QVariant>
#include <Qt>
#include <QtLogging>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

#include <memory>
#include <utility>

namespace {

constexpr qint64 kMaxSumsSize = 1024LL * 1024;          // SHA256SUMS.txt: a few lines
constexpr qint64 kMaxPackageSize = 1024LL * 1024 * 1024; // when the list of releases gives no size
constexpr int kHttpOk = 200;
constexpr int kHttpsPort = 443;
constexpr int kPercent = 100;
constexpr int kMillisecondsPerSecond = 1000;

bool launchDetached(const QString& program, const QStringList& arguments, const QProcessEnvironment& environment)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessEnvironment(environment);
    return process.startDetached();
}

bool sameOrigin(const QUrl& url, const QUrl& origin)
{
    return url.isValid() && origin.isValid() && !origin.host().isEmpty() && url.scheme() == origin.scheme()
           && url.host() == origin.host() && url.port() == origin.port() && url.userInfo().isEmpty();
}

// The test server: an address of this computer. The list of GitHub
// (api.github.com) is not one, so this rule never applies to the app.
bool isLoopback(const QUrl& url)
{
    const QHostAddress address(url.host());
    return url.host() == QLatin1String("localhost") || (!address.isNull() && address.isLoopback());
}

bool isHttpsWithDefaultPort(const QUrl& url)
{
    return url.isValid() && url.scheme() == QLatin1String("https") && url.userInfo().isEmpty()
           && (url.port() == -1 || url.port() == kHttpsPort);
}

} // namespace

UpdateInstaller::UpdateInstaller(QNetworkAccessManager* network, QObject* parent)
    : QObject(parent)
    , m_network(network)
    , m_launcher(launchDetached)
    , m_quit([]() { QCoreApplication::quit(); })
{
    m_installWait.setSingleShot(true);
    m_installWait.setInterval(kInstallWaitMs);
    connect(&m_installWait, &QTimer::timeout, this, [this]() {
        if (m_state == State::Installing) {
            // A second setup stops at once while the first one runs (the
            // SetupMutex of CMakeLists.txt), so a new click is safe.
            setState(State::Ready, QStringLiteral("The setup has not closed the app yet. If a window asks for "
                                                  "administrator rights, accept it. If the setup ended, click "
                                                  "\"Install and restart\" again."));
        }
    });
}

UpdateInstaller::~UpdateInstaller()
{
    stopReply();
}

QString UpdateInstaller::stateName() const
{
    switch (m_state) {
    case State::Idle:        return QStringLiteral("idle");
    case State::Downloading: return QStringLiteral("downloading");
    case State::Ready:       return QStringLiteral("ready");
    case State::Installed:   return QStringLiteral("installed");
    case State::Installing:  return QStringLiteral("installing");
    case State::Failed:      return QStringLiteral("failed");
    }
    return {};
}

QStringList UpdateInstaller::setupArguments()
{
    // https://jrsoftware.org/ishelp/topic_setupcmdline.htm
    //   /SILENT: no questions, a progress window only.
    //   /SUPPRESSMSGBOXES: no message box (their default answer).
    //   /NORESTART: never restart Windows.
    //   /CLOSEAPPLICATIONS: the Restart Manager closes the app, which uses the
    //     files of the installation folder.
    //   /NORESTARTAPPLICATIONS: the Restart Manager does not start it again;
    //     the setup does, as the user (update-restart.iss).
    //   /RESTARTAPP=1: the parameter of update-restart.iss.
    return {QStringLiteral("/SILENT"),
            QStringLiteral("/SUPPRESSMSGBOXES"),
            QStringLiteral("/NORESTART"),
            QStringLiteral("/CLOSEAPPLICATIONS"),
            QStringLiteral("/NORESTARTAPPLICATIONS"),
            QStringLiteral("/RESTARTAPP=1")};
}

bool UpdateInstaller::isProjectDownload(const QUrl& url)
{
    if (!isHttpsWithDefaultPort(url) || url.host() != QLatin1String("github.com")) {
        return false;
    }
    // Encoded and with "." and ".." resolved, as for UpdateController::isProjectPage().
    const QString path = url.adjusted(QUrl::NormalizePathSegments).path(QUrl::FullyEncoded);
    return path.startsWith(QString::fromLatin1(Update::kProjectPath) + QLatin1String("/releases/download/"),
                           Qt::CaseInsensitive);
}

bool UpdateInstaller::isGitHubDownloadHost(const QUrl& url)
{
    // GitHub redirects the file of a release to its storage, today
    // objects.githubusercontent.com or release-assets.githubusercontent.com.
    return isHttpsWithDefaultPort(url)
           && (url.host() == QLatin1String("github.com") || url.host().endsWith(QLatin1String(".githubusercontent.com")));
}

QProcessEnvironment UpdateInstaller::environmentForNewAppImage(const QProcessEnvironment& environment)
{
    // The AppImage runtime sets these again for the new AppImage.
    QProcessEnvironment result = environment;
    const QString appDir = environment.value(QStringLiteral("APPDIR"));
    for (const char* name : {"APPDIR", "APPIMAGE", "ARGV0", "OWD"}) {
        result.remove(QLatin1String(name));
    }
    if (appDir.isEmpty()) {
        return result;
    }
    // The AppRun of packaging/linux/make-appimage.sh puts the folders of its
    // AppImage in these variables: the folders of the old AppImage go.
    const auto inAppDir = [&appDir](const QString& path) {
        return path == appDir || path.startsWith(appDir + QLatin1Char('/'));
    };
    for (const char* name : {"LD_LIBRARY_PATH", "XDG_DATA_DIRS", "QT_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH"}) {
        const QString key = QLatin1String(name);
        if (!result.contains(key)) {
            continue;
        }
        QStringList kept;
        for (const QString& entry : result.value(key).split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
            if (!inAppDir(entry)) {
                kept.append(entry);
            }
        }
        if (kept.isEmpty()) {
            result.remove(key);
        } else {
            result.insert(key, kept.join(QLatin1Char(':')));
        }
    }
    return result;
}

void UpdateInstaller::removeOldDownloads(const QString& dir, bool keepLog)
{
    if (dir.isEmpty()) {
        return;
    }
    const QFileInfoList files = QDir(dir).entryInfoList(QDir::Files | QDir::Hidden);
    for (const QFileInfo& file : files) {
        if (keepLog && file.fileName() == QLatin1String(kSetupLogName)) {
            continue;
        }
        QFile::remove(file.absoluteFilePath());
    }
}

bool UpdateInstaller::allowed(const QUrl& url) const
{
    return isProjectDownload(url) || (isLoopback(m_allowedOrigin) && sameOrigin(url, m_allowedOrigin));
}

bool UpdateInstaller::allowedRedirect(const QUrl& url) const
{
    return isGitHubDownloadHost(url) || (isLoopback(m_allowedOrigin) && sameOrigin(url, m_allowedOrigin));
}

void UpdateInstaller::start(const Update::Release& release, const QString& packageName, const QString& version)
{
    if (m_state != State::Idle && m_state != State::Failed) {
        return;
    }
    m_version = version;
    m_packageName = packageName;
    if (!Update::installsItself(m_kind) || packageName.isEmpty()) {
        fail(QStringLiteral("This copy of the app updates by hand: see the steps on the Updates page."));
        return;
    }
    const Update::ReleaseAsset* package = nullptr;
    const Update::ReleaseAsset* sums = nullptr;
    for (const Update::ReleaseAsset& asset : release.assets) {
        if (asset.name == packageName) {
            package = &asset;
        } else if (asset.name == QLatin1String(kSumsFileName)) {
            sums = &asset;
        }
    }
    if (package == nullptr) {
        fail(QStringLiteral("Version %1 has no file %2. Download the update from the release page.")
                 .arg(version, packageName));
        return;
    }
    if (sums == nullptr) {
        fail(QStringLiteral("Version %1 has no %2, so the download cannot be checked. Download the update from "
                            "the release page.")
                 .arg(version, QLatin1String(kSumsFileName)));
        return;
    }
    if (!allowed(package->downloadUrl) || !allowed(sums->downloadUrl)) {
        fail(QStringLiteral("The files of version %1 are not on the GitHub page of the project: the app does not "
                            "download them.")
                 .arg(version));
        return;
    }
    if (sums->size > kMaxSumsSize) {
        fail(QStringLiteral("The %1 of version %2 is too big for a list of checksums.")
                 .arg(QLatin1String(kSumsFileName), version));
        return;
    }
    QString target;
    if (m_kind == Update::InstallKind::AppImage) {
        target = QFileInfo(m_appImagePath).absoluteFilePath();
    } else {
        removeOldDownloads(m_downloadDir, false);
        if (m_downloadDir.isEmpty() || !QDir().mkpath(m_downloadDir)) {
            fail(QStringLiteral("The app cannot make the folder %1 for the setup.")
                     .arg(QDir::toNativeSeparators(m_downloadDir)));
            return;
        }
        target = QDir(m_downloadDir).filePath(packageName);
    }
    // QSaveFile writes next to the target, and commit() puts the file in its
    // place in one step (rename) after the check. A running AppImage keeps
    // its old file. The new file gets the permissions of the old one. Open it
    // now: a folder or a file that the app cannot write fails before any
    // download.
    m_file = std::make_unique<QSaveFile>(target);
    if (!m_file->open(QIODevice::WriteOnly)) {
        const QString reason = m_file->errorString();
        if (m_kind == Update::InstallKind::AppImage) {
            fail(QStringLiteral("The app cannot replace its AppImage %1 (%2). Download the new AppImage from the "
                                "release page and replace the file by hand.")
                     .arg(QDir::toNativeSeparators(target), reason));
        } else {
            fail(QStringLiteral("The app cannot write the setup %1 (%2).").arg(QDir::toNativeSeparators(target), reason));
        }
        return;
    }
    m_packageUrl = package->downloadUrl;
    m_packageSize = package->size;
    m_expectedSha256.clear();
    m_received = 0;
    m_lastPercent = -1;
    m_hash.reset();
    m_progress = 0.0;
    setState(State::Downloading, QStringLiteral("Downloading version %1…").arg(version));
    QNetworkReply* reply = get(sums->downloadUrl);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { sumsFinished(reply); });
}

QNetworkReply* UpdateInstaller::get(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
    request.setRawHeader("Accept-Language", "en");
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    // Each redirect must go to GitHub (redirected(), below).
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    request.setTransferTimeout(m_timeoutMs);
    QNetworkReply* reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl& target) {
        if (allowedRedirect(target)) {
            emit reply->redirectAllowed();
        } else {
            fail(QStringLiteral("GitHub sent the download of version %1 to %2, which is not GitHub: the app "
                                "stopped it.")
                     .arg(m_version, target.host()));
        }
    });
    return reply;
}

QString UpdateInstaller::downloadError(QNetworkReply* reply, const QString& fileName) const
{
    if (reply->error() == QNetworkReply::OperationCanceledError || reply->error() == QNetworkReply::TimeoutError) {
        return QStringLiteral("The download of %1 stopped: no data came for %2 seconds. Try again.")
            .arg(fileName)
            .arg(m_timeoutMs / kMillisecondsPerSecond);
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 0 && status != kHttpOk) {
        return QStringLiteral("The download of %1 failed: the server answered with the status %2.")
            .arg(fileName)
            .arg(status);
    }
    return QStringLiteral("The download of %1 failed (%2).").arg(fileName, reply->errorString());
}

void UpdateInstaller::sumsFinished(QNetworkReply* reply)
{
    if (reply != m_reply) {
        return;
    }
    m_reply = nullptr;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || (status != 0 && status != kHttpOk)) {
        fail(downloadError(reply, QLatin1String(kSumsFileName)));
        return;
    }
    const QByteArray sums = reply->read(kMaxSumsSize + 1);
    if (sums.size() > kMaxSumsSize) {
        fail(QStringLiteral("The %1 of version %2 is too big for a list of checksums.")
                 .arg(QLatin1String(kSumsFileName), m_version));
        return;
    }
    m_expectedSha256 = Update::sha256Of(sums, m_packageName);
    if (m_expectedSha256.isEmpty()) {
        fail(QStringLiteral("The %1 of version %2 does not list %3, so the download cannot be checked.")
                 .arg(QLatin1String(kSumsFileName), m_version, m_packageName));
        return;
    }
    QNetworkReply* package = get(m_packageUrl);
    connect(package, &QNetworkReply::readyRead, this, [this, package]() { packageData(package); });
    connect(package, &QNetworkReply::finished, this, [this, package]() { packageFinished(package); });
}

void UpdateInstaller::packageData(QNetworkReply* reply)
{
    if (reply != m_reply) {
        return;
    }
    // Only the body of the file: not the body of a redirect.
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    if (status != 0 && status != kHttpOk) {
        return;
    }
    const qint64 limit = m_packageSize > 0 ? m_packageSize : kMaxPackageSize;
    if (m_received + data.size() > limit) {
        fail(QStringLiteral("The download of %1 is bigger than the list of releases says: the app stopped it.")
                 .arg(m_packageName));
        return;
    }
    if (m_file->write(data) != data.size()) {
        fail(QStringLiteral("The app cannot write %1 (%2).")
                 .arg(QDir::toNativeSeparators(m_file->fileName()), m_file->errorString()));
        return;
    }
    m_hash.addData(data);
    m_received += data.size();
    if (m_packageSize > 0) {
        m_progress = static_cast<double>(m_received) / static_cast<double>(m_packageSize);
        const int percent = static_cast<int>(m_progress * kPercent);
        if (percent != m_lastPercent) {
            m_lastPercent = percent;
            emit changed();
        }
    }
}

void UpdateInstaller::packageFinished(QNetworkReply* reply)
{
    if (reply != m_reply) {
        return;
    }
    // What readyRead did not give yet. After an error (a timeout), Qt has
    // closed the reply: nothing to read.
    if (reply->error() == QNetworkReply::NoError) {
        packageData(reply);
        if (reply != m_reply) {
            return; // packageData() failed
        }
    }
    m_reply = nullptr;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || (status != 0 && status != kHttpOk)) {
        fail(downloadError(reply, m_packageName));
        return;
    }
    if (m_packageSize > 0 && m_received != m_packageSize) {
        fail(QStringLiteral("The download of %1 is incomplete (%2 of %3 bytes). Try again.")
                 .arg(m_packageName)
                 .arg(m_received)
                 .arg(m_packageSize));
        return;
    }
    if (m_hash.result().toHex() != m_expectedSha256) {
        fail(QStringLiteral("The download of %1 is damaged: its SHA-256 is not the one in %2. Try again.")
                 .arg(m_packageName, QLatin1String(kSumsFileName)));
        return;
    }
    apply();
}

void UpdateInstaller::apply()
{
    const QString target = m_file->fileName();
    if (!m_file->commit()) {
        const QString reason = m_file->errorString();
        m_file.reset();
        if (m_kind == Update::InstallKind::AppImage) {
            fail(QStringLiteral("The new AppImage cannot replace %1 (%2). Download it from the release page and "
                                "replace the file by hand.")
                     .arg(QDir::toNativeSeparators(target), reason));
        } else {
            fail(QStringLiteral("The app cannot write the setup %1 (%2).").arg(QDir::toNativeSeparators(target), reason));
        }
        return;
    }
    m_file.reset();
    m_progress = 1.0;
    if (m_kind == Update::InstallKind::AppImage) {
        setState(State::Installed, QStringLiteral("Version %1 is installed. Restart the app to use it.").arg(m_version));
    } else {
        m_setupPath = target;
        setState(State::Ready, QStringLiteral("Version %1 is ready. The app closes during the installation and opens "
                                              "again.")
                                   .arg(m_version));
    }
}

void UpdateInstaller::cancel()
{
    if (m_state == State::Downloading || m_state == State::Failed) {
        stopReply();
        m_file.reset(); // discards the part that was downloaded
        setState(State::Idle, {});
    }
}

bool UpdateInstaller::setupUnchanged() const
{
    // The setup waited in a folder that the user can write, maybe for hours,
    // and an installation "for all users" runs it as an administrator: check
    // it again just before it runs.
    QFile file(m_setupPath);
    if (!file.open(QIODevice::ReadOnly) || (m_packageSize > 0 && file.size() != m_packageSize)) {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && hash.result().toHex() == m_expectedSha256;
}

void UpdateInstaller::restart()
{
    if (m_state == State::Ready) {
        if (!setupUnchanged()) {
            fail(QStringLiteral("The setup of version %1 changed after the download: the app does not run it. "
                                "Click \"Install\" to download it again.")
                     .arg(m_version));
            return;
        }
        QStringList arguments = setupArguments();
        // The log of the setup, for a failed update.
        arguments.append(QStringLiteral("/LOG=%1").arg(
            QDir::toNativeSeparators(QDir(m_downloadDir).filePath(QLatin1String(kSetupLogName)))));
        if (!m_launcher(m_setupPath, arguments, QProcessEnvironment::systemEnvironment())) {
            fail(QStringLiteral("The setup of version %1 did not start. Run it by hand: %2")
                     .arg(m_version, QDir::toNativeSeparators(m_setupPath)));
            return;
        }
        setState(State::Installing, QStringLiteral("Installing version %1. The app closes now and opens again when "
                                                   "the installation ends.")
                                        .arg(m_version));
        m_installWait.start();
        return;
    }
    if (m_state == State::Installed) {
        const QProcessEnvironment environment = environmentForNewAppImage(QProcessEnvironment::systemEnvironment());
        if (!m_launcher(QFileInfo(m_appImagePath).absoluteFilePath(), {}, environment)) {
            fail(QStringLiteral("Version %1 is installed, but it did not start. Start the app again by hand.")
                     .arg(m_version));
            return;
        }
        m_quit();
    }
}

void UpdateInstaller::fail(const QString& message)
{
    stopReply();
    m_file.reset(); // discards the part that was downloaded
    qWarning().noquote() << "[UpdateInstaller]" << message;
    setState(State::Failed, message);
}

void UpdateInstaller::setState(State state, const QString& message)
{
    m_state = state;
    m_message = message;
    if (state != State::Installing) {
        m_installWait.stop();
    }
    emit changed();
}

void UpdateInstaller::stopReply()
{
    if (m_reply == nullptr) {
        return;
    }
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    // No more signal to this object (abort() emits finished()).
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}
