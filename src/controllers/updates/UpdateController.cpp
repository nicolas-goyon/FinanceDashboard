#include "UpdateController.h"

#include "UpdateInstaller.h"
#include "update/ReleaseFeed.h"
#include "update/SemVer.h"
#include "update/UpdatePackage.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QLatin1Char>
#include <QLatin1String>
#include <QList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextFragment>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <Qt>
#include <QtGlobal>
#include <QtTypes>
#include <QtLogging>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

#include <algorithm>

namespace {

constexpr int kHttpOk = 200;
constexpr int kHttpForbidden = 403;
constexpr int kHttpTooManyRequests = 429;
constexpr int kMillisecondsPerSecond = 1000;

const char* const kKeyCheckAtStart = "updates/checkAtStart";
const char* const kKeyChannel = "updates/channel";
const char* const kKeyDismissed = "updates/dismissedVersion";

QString channelLabel(Update::Channel channel)
{
    switch (channel) {
    case Update::Channel::Stable:       return QStringLiteral("Stable");
    case Update::Channel::Experimental: return QStringLiteral("Experimental");
    case Update::Channel::Preprod:      return QStringLiteral("Preprod");
    }
    return {};
}

// A text document that reads no resource (an image, a style sheet of HTML
// in the Markdown), and records that something asked for one: safeNotes()
// reads text from GitHub.
class NoResourceDocument : public QTextDocument
{
public:
    using QTextDocument::QTextDocument;
    [[nodiscard]] bool askedForResource() const { return m_askedForResource; }

protected:
    QVariant loadResource(int /*type*/, const QUrl& /*name*/) override
    {
        m_askedForResource = true;
        return {};
    }

private:
    bool m_askedForResource = false;
};

// True when Text.MarkdownText can show `markdown` without reading a file or
// an address. The same parser as QQuickText (setMarkdown(), GitHub dialect,
// with HTML) must make no image and no background image, and ask for no
// style sheet.
bool readsNoResource(const QString& markdown)
{
    NoResourceDocument document;
    document.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    if (document.askedForResource()) {
        return false;
    }
    const QList<QTextFormat> formats = document.allFormats();
    return std::none_of(formats.cbegin(), formats.cend(), [](const QTextFormat& format) {
        return format.isImageFormat() || format.hasProperty(QTextFormat::BackgroundImageUrl);
    });
}

// `text` as Markdown with no Markdown and no HTML in it: a backslash before
// each ASCII punctuation character (CommonMark § 2.4), one paragraph for
// each line.
QString plainMarkdown(const QString& text)
{
    static const QLatin1String punctuation("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
    QString result;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        QString escaped;
        for (const QChar character : line) {
            if (punctuation.contains(character)) {
                escaped += QLatin1Char('\\');
            }
            escaped += character;
        }
        if (!escaped.trimmed().isEmpty()) {
            result += escaped + QLatin1String("\n\n");
        }
    }
    return result;
}

// Without a token, GitHub allows 60 calls an hour for each internet address.
// Over the limit it answers 403 or 429, with the time to wait in retry-after
// (seconds), or with x-ratelimit-remaining: 0 and the end of the limit in
// x-ratelimit-reset (seconds since 1970).
// https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api
bool isRateLimited(const QNetworkReply* reply, int status)
{
    return status == kHttpTooManyRequests
           || (status == kHttpForbidden
               && (reply->rawHeader("x-ratelimit-remaining") == "0" || reply->hasRawHeader("retry-after")));
}

QString rateLimitMessage(const QNetworkReply* reply)
{
    QDateTime until;
    bool known = false;
    const qint64 wait = reply->rawHeader("retry-after").toLongLong(&known);
    if (known) {
        until = QDateTime::currentDateTime().addSecs(wait);
    } else {
        const qint64 reset = reply->rawHeader("x-ratelimit-reset").toLongLong(&known);
        if (known) {
            until = QDateTime::fromSecsSinceEpoch(reset);
        }
    }
    if (until.isValid()) {
        return QStringLiteral("GitHub refuses more checks until %1: it limits the calls of each internet "
                              "address (60 an hour). Try again after that time.")
            .arg(until.toLocalTime().toString(QStringLiteral("HH:mm")));
    }
    return QStringLiteral("GitHub refuses more checks for now: it limits the calls of each internet "
                          "address (60 an hour). Try again later.");
}

} // namespace

UpdateController::UpdateController(QObject* parent)
    : QObject(parent)
    , m_feedUrl(QString::fromLatin1(kDefaultFeedUrl))
    , m_currentVersion(QCoreApplication::applicationVersion())
    , m_checkAtStart(QSettings().value(QLatin1String(kKeyCheckAtStart), true).toBool())
{
#ifdef Q_OS_WIN
    constexpr bool kWindows = true;
#else
    constexpr bool kWindows = false;
#endif
    // How this copy was installed decides what "Install" does.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    m_installer.setKind(Update::installKindOf(QDir(QCoreApplication::applicationDirPath()), appImage, kWindows));
    m_installer.setAppImagePath(appImage);
    // The setup waits in a folder of the temporary folder, one for each
    // identity. A setup of an earlier update goes now (or at the next start,
    // when the setup still runs); its log stays until the next download.
    m_installer.setDownloadDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1Char('/')
                               + QCoreApplication::applicationName() + QLatin1String("-update"));
    if (m_installer.kind() == Update::InstallKind::WindowsSetup) {
        UpdateInstaller::removeOldDownloads(m_installer.downloadDir(), true);
    }
    m_installer.setUserAgent(QStringLiteral("FinanceDashboard/%1").arg(m_currentVersion).toUtf8());
    m_installer.setAllowedOrigin(m_feedUrl);
    connect(&m_installer, &UpdateInstaller::changed, this, [this]() {
        emit installChanged();
        emit stateChanged(); // bannerVisible
    });
}

void UpdateController::setTestApp(bool testApp)
{
    if (testApp == m_testApp) {
        return;
    }
    m_testApp = testApp;
    emit settingsChanged();
    applyChannel();
}

void UpdateController::setCheckAtStart(bool check)
{
    if (check == m_checkAtStart) {
        return;
    }
    m_checkAtStart = check;
    QSettings().setValue(QLatin1String(kKeyCheckAtStart), check);
    emit settingsChanged();
}

QString UpdateController::channel() const
{
    if (!m_testApp) {
        return QStringLiteral("stable");
    }
    const QString value = QSettings().value(QLatin1String(kKeyChannel)).toString();
    return value == QLatin1String("preprod") ? value : QStringLiteral("experimental");
}

void UpdateController::setChannel(const QString& channel)
{
    if (!m_testApp || channel == this->channel()
        || (channel != QLatin1String("experimental") && channel != QLatin1String("preprod"))) {
        return;
    }
    QSettings().setValue(QLatin1String(kKeyChannel), channel);
    emit settingsChanged();
    applyChannel();
}

QVariantList UpdateController::channels() const
{
    if (!m_testApp) {
        return {QVariantMap{{QStringLiteral("id"), QStringLiteral("stable")},
                            {QStringLiteral("label"), QStringLiteral("Stable")}}};
    }
    return {QVariantMap{{QStringLiteral("id"), QStringLiteral("experimental")},
                        {QStringLiteral("label"), QStringLiteral("Experimental (-exp and -rc builds)")}},
            QVariantMap{{QStringLiteral("id"), QStringLiteral("preprod")},
                        {QStringLiteral("label"), QStringLiteral("Preprod (-rc builds)")}}};
}

Update::Channel UpdateController::currentChannel() const
{
    const QString channelId = channel();
    if (channelId == QLatin1String("experimental")) {
        return Update::Channel::Experimental;
    }
    if (channelId == QLatin1String("preprod")) {
        return Update::Channel::Preprod;
    }
    return Update::Channel::Stable;
}

void UpdateController::setFeedUrl(const QUrl& url)
{
    if (url == m_feedUrl) {
        return;
    }
    m_feedUrl = url;
    m_installer.setAllowedOrigin(url);
    emit settingsChanged();
}

bool UpdateController::bannerVisible() const
{
    // An installation in progress, or its failure, always shows.
    if (m_installer.state() != UpdateInstaller::State::Idle) {
        return true;
    }
    if (!updateAvailable()) {
        return false;
    }
    // Hidden for the version whose banner the user closed, and for the older
    // ones: back for a newer one only.
    const Update::SemVer dismissed =
        Update::parseSemVer(QSettings().value(QLatin1String(kKeyDismissed)).toString());
    return !dismissed.valid || Update::compareSemVer(Update::parseSemVer(m_latestNewer), dismissed) > 0;
}

void UpdateController::dismissBanner()
{
    // A failed installation closes with its banner; an installation in
    // progress does not.
    if (m_installer.state() == UpdateInstaller::State::Failed) {
        m_installer.cancel();
    } else if (m_installer.state() != UpdateInstaller::State::Idle) {
        return;
    }
    if (!updateAvailable()) {
        return;
    }
    QSettings().setValue(QLatin1String(kKeyDismissed), m_latestNewer);
    emit stateChanged();
}

void UpdateController::startupCheck()
{
    if (m_startupCheckDone) {
        return;
    }
    m_startupCheckDone = true;
    if (m_checkAtStart) {
        // The user can turn the check off during the delay.
        QTimer::singleShot(m_startupDelayMs, this, [this]() {
            if (m_checkAtStart) {
                check();
            }
        });
    }
}

void UpdateController::check()
{
    if (m_reply != nullptr) {
        return;
    }
    QNetworkRequest request(m_feedUrl);
    // https://docs.github.com/en/rest/using-the-rest-api/getting-started-with-the-rest-api
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FinanceDashboard/%1").arg(m_currentVersion));
    // Else Qt sends the language of the computer (Accept-Language: fr-FR,en,*).
    request.setRawHeader("Accept-Language", "en");
    // No cookie: Qt neither sends nor keeps one for this request (the
    // default cookie jar would keep a cookie of GitHub for the next check).
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    // No data for this time: Qt stops the transfer (OperationCanceledError).
    request.setTransferTimeout(m_timeoutMs);
    m_reply = m_network.get(request);
    QNetworkReply* reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { finish(reply); });
    m_statusText = QStringLiteral("Checking…");
    m_statusIsError = false;
    emit stateChanged();
}

void UpdateController::finish(QNetworkReply* reply)
{
    reply->deleteLater();
    m_reply = nullptr;

    // A file:// address (the tests) has no HTTP status.
    const QVariant statusValue = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const int status = statusValue.isValid() ? statusValue.toInt() : 0;
    if (isRateLimited(reply, status)) {
        fail(rateLimitMessage(reply));
        return;
    }
    // The transfer timeout of check(): nothing else stops the reply.
    if (reply->error() == QNetworkReply::OperationCanceledError || reply->error() == QNetworkReply::TimeoutError) {
        fail(QStringLiteral("The check failed: GitHub did not answer within %1 seconds. Try again later.")
                 .arg(m_timeoutMs / kMillisecondsPerSecond));
        return;
    }
    if (status != 0 && status != kHttpOk) {
        fail(QStringLiteral("The check failed: GitHub answered with the status %1.").arg(status));
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("The check failed: GitHub could not be reached (%1).").arg(reply->errorString()));
        return;
    }
    QString error;
    QVector<Update::Release> releases = Update::parseReleases(reply->readAll(), &error);
    if (!error.isEmpty()) {
        fail(QStringLiteral("The check failed: the answer of GitHub cannot be read (%1).").arg(error));
        return;
    }
    // Once for each list, not at each change of channel.
    for (Update::Release& release : releases) {
        release.notes = safeNotes(release.notes);
    }
    m_allReleases = releases;
    m_listRead = true;
    m_checkedAt = QDateTime::currentDateTime();
    applyChannel();
    emit checkFinished();
}

void UpdateController::fail(const QString& message)
{
    m_statusText = message;
    m_statusIsError = true;
    qWarning().noquote() << "[UpdateController]" << message;
    emit stateChanged();
    emit checkFinished();
}

void UpdateController::applyChannel()
{
    if (!m_listRead) {
        emit stateChanged();
        return;
    }
    const Update::Channel channelNow = currentChannel();
    const QVector<Update::Release> releases =
        Update::releasesOf(m_allReleases, channelNow, QCoreApplication::applicationName());
    const Update::SemVer current = Update::parseSemVer(m_currentVersion);

    m_releaseRows.clear();
    m_latestNewer.clear();
    m_latestRelease = {};
    for (const Update::Release& release : releases) {
        QString relation;
        if (current.valid) {
            const int order = Update::compareSemVer(release.version, current);
            if (order > 0) {
                relation = QStringLiteral("newer");
            } else if (order == 0) {
                relation = QStringLiteral("current");
            } else {
                relation = QStringLiteral("older");
            }
        }
        m_releaseRows.append(QVariantMap{
            {QStringLiteral("tag"), release.tag},
            {QStringLiteral("version"), Update::semVerText(release.version)},
            {QStringLiteral("title"), release.title},
            {QStringLiteral("date"), release.published.isValid()
                                         ? release.published.toLocalTime().toString(QStringLiteral("yyyy-MM-dd"))
                                         : QString()},
            {QStringLiteral("notes"), release.notes},
            {QStringLiteral("page"), release.page.toString()},
            {QStringLiteral("relation"), relation},
        });
    }
    if (!releases.isEmpty() && current.valid && Update::compareSemVer(releases.first().version, current) > 0) {
        m_latestNewer = Update::semVerText(releases.first().version);
        m_latestRelease = releases.first();
    }

    // During a check (the user changed the channel), "Checking…" stays until
    // the check ends.
    if (checking()) {
        emit stateChanged();
        return;
    }
    const QString checked = m_checkedAt.toString(QStringLiteral("HH:mm"));
    if (releases.isEmpty()) {
        m_statusText = QStringLiteral("There is no version in the %1 channel yet. Checked at %2.")
                           .arg(channelLabel(channelNow), checked);
    } else if (!m_latestNewer.isEmpty()) {
        m_statusText = QStringLiteral("Version %1 is available. This app is version %2. Checked at %3.")
                           .arg(m_latestNewer, m_currentVersion, checked);
    } else {
        m_statusText = QStringLiteral("This app is up to date (version %1). Checked at %2.")
                           .arg(m_currentVersion, checked);
    }
    m_statusIsError = false;
    emit stateChanged();
}

bool UpdateController::isProjectPage(const QUrl& url)
{
    // "." and ".." first, so a path cannot go out of the project. QUrl keeps
    // the scheme and the host in lower case.
    const QUrl page = url.adjusted(QUrl::NormalizePathSegments);
    constexpr int kHttpsPort = 443;
    if (!page.isValid() || page.scheme() != QLatin1String("https") || page.host() != QLatin1String("github.com")
        || !page.userInfo().isEmpty() || (page.port() != -1 && page.port() != kHttpsPort)) {
        return false;
    }
    const QString project = QString::fromLatin1(kProjectPagePath);
    // Encoded, as the browser gets it: %2F stays in its path segment.
    const QString path = page.path(QUrl::FullyEncoded);
    return path.compare(project, Qt::CaseInsensitive) == 0
           || path.startsWith(project + QLatin1Char('/'), Qt::CaseInsensitive);
}

QString UpdateController::safeNotes(const QString& markdown)
{
    if (markdown.isEmpty() || readsNoResource(markdown)) {
        return markdown; // usually: the notes as GitHub gives them
    }
    // Remove the images: the parser makes an image fragment of each one,
    // also of a reference image and of an <img> tag. toMarkdown() writes no
    // HTML, so no style sheet and no background image either.
    NoResourceDocument document;
    document.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    QList<QTextFragment> images;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.isValid() && fragment.charFormat().isImageFormat()) {
                images.append(fragment);
            }
        }
    }
    // From the end: a removal does not move the fragments before it.
    QTextCursor cursor(&document);
    for (auto it = images.crbegin(); it != images.crend(); ++it) {
        cursor.setPosition(it->position());
        cursor.setPosition(it->position() + it->length(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
    QString withoutImages = document.toMarkdown(QTextDocument::MarkdownDialectGitHub);
    if (readsNoResource(withoutImages)) {
        return withoutImages;
    }
    // toMarkdown() does not escape all text: HTML written as text in the
    // notes (&lt;img ...&gt;) can come back as HTML. Then the text only.
    const QString plain = plainMarkdown(document.toPlainText());
    return readsNoResource(plain) ? plain : QString();
}

void UpdateController::install()
{
    if (!updateAvailable() || !installsItself()) {
        return;
    }
    m_installer.start(m_latestRelease,
                      Update::packageFileName(m_installer.kind(), QCoreApplication::applicationName(), m_latestNewer),
                      m_latestNewer);
}

void UpdateController::cancelInstall()
{
    m_installer.cancel();
}

void UpdateController::restartNow()
{
    m_installer.restart();
}

QString UpdateController::manualSteps() const
{
    if (!updateAvailable() || installsItself()) {
        return {};
    }
    const QString appName = QCoreApplication::applicationName();
    const Update::InstallKind kind = m_installer.kind();
    switch (kind) {
    case Update::InstallKind::WindowsPortable:
        return QStringLiteral("This copy of the app is the portable ZIP: update it by hand. Download %1 from the "
                              "release page, close the app, then extract the ZIP over the folder of the app. Your "
                              "data is not in that folder.")
            .arg(Update::packageFileName(kind, appName, m_latestNewer));
    case Update::InstallKind::LinuxPackage:
        return QStringLiteral("This copy of the app comes from the .deb: update it by hand. Download %1 from the "
                              "release page, then run: sudo apt install ./%1")
            .arg(Update::packageFileName(kind, appName, m_latestNewer));
    case Update::InstallKind::LinuxArchive:
        if (m_testApp) {
            // The Test app has no .tar.gz: its Linux package is the AppImage.
            return QStringLiteral("This copy of the Test app is not an AppImage: update it by hand. Download %1 from "
                                  "the release page and start it.")
                .arg(Update::packageFileName(Update::InstallKind::AppImage, appName, m_latestNewer));
        }
        return QStringLiteral("This copy of the app comes from the .tar.gz (or from a build): update it by hand. "
                              "Download %1 from the release page, close the app, then extract it over the old "
                              "folder. Your data is not in that folder.")
            .arg(Update::packageFileName(kind, appName, m_latestNewer));
    case Update::InstallKind::WindowsSetup:
    case Update::InstallKind::AppImage:
        break;
    }
    return {};
}

void UpdateController::openPage(const QString& url)
{
    // The release pages of the list, and the links of their notes: the pages
    // of this project only.
    const QUrl page(url);
    if (!isProjectPage(page)) {
        qWarning().noquote() << "[UpdateController] not a page of the project on GitHub, not opened:" << url;
        return;
    }
    QDesktopServices::openUrl(page);
}
