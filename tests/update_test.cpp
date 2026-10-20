// update_test — the update check (#117): versions, the list of releases,
// UpdateController and the network rule (ARCHITECTURE.MD § Network rule),
// with no internet: a file:// list, or a local test server (FakeGitHub, on
// 127.0.0.1) in place of api.github.com.
//
//   - SemVer: the precedence of SemVer 2.0.0 (the example of semver.org) and
//     the tags of the release channels (#129); invalid texts;
//   - the list of releases: a draft, a tag that is not a version and a JSON
//     error are left out; each channel keeps its releases, only with a package
//     of the app that asks, newest first;
//   - UpdateController, with a file:// list: the versions of the channel and
//     their relation to the version of the app, the update and the banner
//     (closed for one version and the older ones, back for a newer one), the
//     change of channel without a new download, the settings, and a failed
//     check that keeps the app working;
//   - UpdateController, with FakeGitHub: the request and its headers (nothing
//     else), the answers of a GitHub that refuses (rate limits, errors) or
//     does not answer, the check at start (once, or not when turned off), a
//     change of channel during a check;
//   - openPage(): the pages of the project on GitHub only;
//   - safeNotes(): notes that a Text shows without reading a file or an
//     address (no image, no background image, no style sheet), the usual
//     notes unchanged;
//   - the installation (#117, part 2): the kind of a copy of the app, the
//     names of the packages, SHA256SUMS.txt, the addresses that the
//     installer downloads; with FakeGitHub: the AppImage replaced in place
//     and started again, the setup kept until "Install and restart" (its
//     command line), a damaged, bigger, incomplete or redirected download
//     refused with the old file unchanged, a cancelled download, a new try
//     after a failure, and the steps of the copies that update by hand;
//   - the network access of the QML engine (Startup::LocalOnlyNetworkFactory):
//     local files only, and an image in Markdown notes is not downloaded.

#include "UpdateController.h"
#include "UpdateInstaller.h"
#include "startup/LocalOnlyNetwork.h"
#include "update/ReleaseFeed.h"
#include "update/SemVer.h"
#include "update/UpdatePackage.h"

#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QPointer>
#include <QProcessEnvironment>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QTextFormat>
#include <QTime>
#include <QTimeZone>
#include <QUrl>
#include <QVariantMap>

#include <memory>
#include <tuple>
#include <utility>

namespace {

// Six releases as GitHub lists them (tests/data/update_releases.json): the
// prod release v0.2.1 (prod packages), a release candidate and two
// experimental builds of 0.2.2 (Test packages), a draft and a tag that is not
// a version. In a file: moc does not read raw string literals.
QByteArray releasesJson()
{
    QFile file(QFINDTESTDATA("data/update_releases.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("tests/data/update_releases.json is missing");
    }
    return file.readAll();
}

QStringList versionsOf(const QVector<Update::Release>& releases)
{
    QStringList versions;
    for (const Update::Release& release : releases) {
        versions.append(Update::semVerText(release.version));
    }
    return versions;
}

QStringList rowVersions(const QVariantList& rows)
{
    QStringList versions;
    for (const QVariant& row : rows) {
        versions.append(row.toMap().value(QStringLiteral("version")).toString() + QLatin1Char(':')
                        + row.toMap().value(QStringLiteral("relation")).toString());
    }
    return versions;
}

// A whole HTTP/1.1 answer. `headers`: more header lines, each with its "\r\n".
QByteArray httpAnswer(int status, const QByteArray& reason, const QByteArray& body, const QByteArray& headers = {},
                      const QByteArray& contentType = "application/json; charset=utf-8")
{
    return "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n"
           + "Content-Type: " + contentType + "\r\n"
           + "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
           + "Connection: close\r\n" + headers + "\r\n" + body;
}

// A PNG image of 1 × 1 pixel: an image that QML can show.
QByteArray pngImage()
{
    QImage image(1, 1, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}

// The headers of a request head, with the names in lower case (HTTP names
// ignore the case).
QHash<QByteArray, QByteArray> headersOf(const QByteArray& head)
{
    QHash<QByteArray, QByteArray> headers;
    const QList<QByteArray> lines = head.split('\n');
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon > 0) {
            headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
        }
    }
    return headers;
}

// A document that records each resource that the Markdown parser asks for.
class ResourceRecorder : public QTextDocument
{
public:
    bool asked = false;

protected:
    QVariant loadResource(int /*type*/, const QUrl& /*name*/) override
    {
        asked = true;
        return {};
    }
};

// What safeNotes() promises, checked again with the parser of Text
// (setMarkdown(), GitHub dialect, with HTML): no image, no background image,
// no style sheet.
bool readsNoResource(const QString& markdown)
{
    ResourceRecorder document;
    document.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    if (document.asked) {
        return false;
    }
    const QList<QTextFormat> formats = document.allFormats();
    for (const QTextFormat& format : formats) {
        if (format.isImageFormat() || format.hasProperty(QTextFormat::BackgroundImageUrl)) {
            return false;
        }
    }
    return true;
}

// The request path of the list of releases (FakeGitHub::feedUrl()).
const QByteArray kFeedPath = "/repos/nicolas-goyon/FinanceDashboard/releases?per_page=100";

QByteArray packageBytes(int size)
{
    QByteArray bytes;
    bytes.reserve(size);
    for (int i = 0; i < size; ++i) {
        bytes.append(static_cast<char>((i * 7) % 251));
    }
    return bytes;
}

QByteArray sha256Hex(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

// A SHA256SUMS.txt as CI writes it: `hash` for `name`, and another file.
QByteArray sumsFor(const QByteArray& hash, const QString& name)
{
    return hash + "  " + name.toUtf8() + "\n" + QByteArray(64, 'a') + "  FinanceDashboard-0.2.3-source.zip\n";
}

// One release of the prod app, `version`, with `assets` (name, address, size).
QByteArray releaseJson(const QString& version, const QList<std::tuple<QString, QString, qint64>>& assets)
{
    QJsonArray files;
    for (const auto& [name, url, size] : assets) {
        files.append(QJsonObject{{QStringLiteral("name"), name},
                                 {QStringLiteral("browser_download_url"), url},
                                 {QStringLiteral("size"), size}});
    }
    const QJsonObject release{
        {QStringLiteral("tag_name"), QStringLiteral("v") + version},
        {QStringLiteral("name"), QStringLiteral("Finance Dashboard v") + version},
        {QStringLiteral("prerelease"), false},
        {QStringLiteral("draft"), false},
        {QStringLiteral("published_at"), QStringLiteral("2026-10-22T08:00:00Z")},
        {QStringLiteral("body"), QStringLiteral("Notes.")},
        {QStringLiteral("html_url"),
         QStringLiteral("https://github.com/nicolas-goyon/FinanceDashboard/releases/tag/v") + version},
        {QStringLiteral("assets"), files},
    };
    return QJsonDocument(QJsonArray{release}).toJson(QJsonDocument::Compact);
}

void writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qFatal("cannot write %s", qPrintable(path));
    }
    file.write(content);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// The prod app 0.2.2, which follows the releases (Stable).
void useProdApp()
{
    QCoreApplication::setApplicationName(QStringLiteral("FinanceDashboard"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.2"));
}

} // namespace

// ── FakeGitHub ───────────────────────────────────────────────────────────────
// A local HTTP server in place of api.github.com, on 127.0.0.1. It reads each
// request head and answers with answer(), then closes the connection. With an
// empty answer it never answers (a GitHub that hangs). With hold(), it keeps
// the answer until answerNow().
class FakeGitHub : public QObject
{
    Q_OBJECT

public:
    FakeGitHub()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &FakeGitHub::accept);
        m_listening = m_server.listen(QHostAddress::LocalHost);
    }

    [[nodiscard]] bool listening() const { return m_listening; }
    [[nodiscard]] quint16 port() const { return m_server.serverPort(); }
    [[nodiscard]] QUrl feedUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/repos/nicolas-goyon/FinanceDashboard/releases?per_page=100")
                        .arg(port()));
    }
    void setAnswer(const QByteArray& answer) { m_answer = answer; }
    // The answer for one request path (path and query), in place of answer().
    void setRoute(const QByteArray& path, const QByteArray& answer) { m_routes.insert(path, answer); }
    void hold(bool hold) { m_hold = hold; }
    // Holds the answers of `path` only.
    void holdPath(const QByteArray& path) { m_holdPath = path; }
    void answerNow()
    {
        const QList<QPair<QPointer<QTcpSocket>, QByteArray>> held = std::exchange(m_held, {});
        for (const auto& [socket, path] : held) {
            if (socket != nullptr) {
                answer(socket, path);
            }
        }
    }
    [[nodiscard]] int connections() const { return m_connections; }
    [[nodiscard]] const QList<QByteArray>& requests() const { return m_requests; }
    // The request paths, in order.
    [[nodiscard]] QList<QByteArray> paths() const
    {
        QList<QByteArray> result;
        for (const QByteArray& head : m_requests) {
            result.append(head.left(head.indexOf('\r')).split(' ').value(1));
        }
        return result;
    }

private:
    void accept()
    {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            ++m_connections;
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { read(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    }
    void read(QTcpSocket* socket)
    {
        QByteArray& data = m_received[socket];
        data += socket->readAll();
        const qsizetype end = data.indexOf("\r\n\r\n");
        if (end < 0) {
            return;
        }
        const QByteArray head = data.left(end);
        m_requests.append(head);
        m_received.remove(socket);
        const QByteArray path = head.left(head.indexOf('\r')).split(' ').value(1);
        if (m_hold || (!m_holdPath.isEmpty() && path == m_holdPath)) {
            m_held.append({socket, path});
            return;
        }
        answer(socket, path);
    }
    void answer(QTcpSocket* socket, const QByteArray& path)
    {
        const QByteArray response = m_routes.value(path, m_answer);
        if (response.isEmpty()) {
            return;
        }
        socket->write(response);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    bool m_listening = false;
    QByteArray m_answer;
    QHash<QByteArray, QByteArray> m_routes;
    bool m_hold = false;
    QByteArray m_holdPath;
    int m_connections = 0;
    QHash<QTcpSocket*, QByteArray> m_received;
    QList<QByteArray> m_requests;
    QList<QPair<QPointer<QTcpSocket>, QByteArray>> m_held;
};

// Serves a release 0.2.3 of the prod app on `github`: the list, its
// SHA256SUMS.txt (/dl/sums) and `package` named `name` (/dl/package), with
// `announced` as its size in the list (-1: its real size).
void serveRelease(FakeGitHub& github, const QString& name, const QByteArray& package, const QByteArray& sums,
                  qint64 announced = -1)
{
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(github.port());
    github.setRoute(kFeedPath, httpAnswer(200, "OK",
                                          releaseJson(QStringLiteral("0.2.3"),
                                                      {{name, base + QStringLiteral("/dl/package"),
                                                        announced < 0 ? package.size() : announced},
                                                       {QStringLiteral("SHA256SUMS.txt"), base + QStringLiteral("/dl/sums"),
                                                        sums.size()}})));
    github.setRoute("/dl/sums", httpAnswer(200, "OK", sums, {}, "text/plain"));
    github.setRoute("/dl/package", httpAnswer(200, "OK", package, {}, "application/octet-stream"));
}

class UpdateTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void semverPrecedence();
    void semverOfTheChannels();
    void invalidVersions();
    void listOfReleases();
    void releasesOfEachChannel();
    void controllerFindsTheNewestVersionOfItsChannel();
    void bannerClosedForOneVersionOnly();
    void prodAppFollowsStable();
    void failedCheckKeepsTheApp();
    void bannerStaysClosedForAnOlderVersion();

    void checkSendsOnlyTheExpectedRequest();
    void refusedChecks_data();
    void refusedChecks();
    void checkWithNoAnswerFails();
    void startupCheckRunsOnce();
    void startupCheckTurnedOff();
    void channelChangeDuringACheck();
    void opensTheProjectPagesOnly_data();
    void opensTheProjectPagesOnly();
    void safeNotes_data();
    void safeNotes();
    void listShowsSafeNotes();

    void qmlNetworkReadsLocalFilesOnly();
    void markdownImageIsNotDownloaded();

    void installKinds();
    void sumsOfARelease();
    void downloadAddresses();
    void environmentOfTheNewAppImage();
    void installReplacesTheAppImage();
    void installKeepsTheSetupUntilRestart();
    void installRefusesABadDownload_data();
    void installRefusesABadDownload();
    void installCanBeCancelled();
    void installFollowsAnAllowedRedirect();
    void installAgainAfterAFailure();
    void installStopsADownloadWithNoData();
    void installChecksTheSetupAgainBeforeItRuns();
    void installWaitsForTheSetupThenOffersAgain();
    void installTellsWhenTheNewVersionDoesNotStart();
    void installNeedsAWritableAppImage();
    void installAllowsTheFeedServerOnlyOnThisComputer();
    void oldDownloadsGo();
    void manualStepsForTheOtherCopies_data();
    void manualStepsForTheOtherCopies();

private:
    // Runs a check of `controller` on the list file, and waits for its end.
    static void checkNow(UpdateController& controller);
    void writeList(const QByteArray& json);

    QTemporaryDir m_dir;
    QString m_listPath;
};

void UpdateTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    // The settings of the test, in the temporary folder: not the settings of
    // a real app.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.filePath(QStringLiteral("settings")));
    QCoreApplication::setOrganizationName(QStringLiteral("DataAnalysisTest"));
    m_listPath = m_dir.filePath(QStringLiteral("releases.json"));
}

void UpdateTest::init()
{
    // The settings of each identity that a test uses: the application name
    // gives the settings file, so set it before clear().
    for (const QString& name : {QStringLiteral("FinanceDashboard"), QStringLiteral("FinanceDashboardTest")}) {
        QCoreApplication::setApplicationName(name);
        QSettings().clear();
    }
    QCoreApplication::setApplicationName(QStringLiteral("FinanceDashboardTest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.2-exp.57"));
    writeList(releasesJson());
}

void UpdateTest::writeList(const QByteArray& json)
{
    QFile file(m_listPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(json);
}

void UpdateTest::checkNow(UpdateController& controller)
{
    QSignalSpy finished(&controller, &UpdateController::checkFinished);
    controller.check();
    QVERIFY(controller.checking());
    QVERIFY(finished.wait(5000));
    QVERIFY(!controller.checking());
}

void UpdateTest::semverPrecedence()
{
    // https://semver.org/ § 11: each one is lower than the next.
    const QStringList ordered = {
        QStringLiteral("1.0.0-alpha"), QStringLiteral("1.0.0-alpha.1"), QStringLiteral("1.0.0-alpha.beta"),
        QStringLiteral("1.0.0-beta"), QStringLiteral("1.0.0-beta.2"), QStringLiteral("1.0.0-beta.11"),
        QStringLiteral("1.0.0-rc.1"), QStringLiteral("1.0.0"), QStringLiteral("1.0.1"), QStringLiteral("1.1.0"),
        QStringLiteral("2.0.0"), QStringLiteral("10.0.0"),
    };
    for (qsizetype i = 0; i + 1 < ordered.size(); ++i) {
        const Update::SemVer lower = Update::parseSemVer(ordered.at(i));
        const Update::SemVer higher = Update::parseSemVer(ordered.at(i + 1));
        QVERIFY2(lower.valid && higher.valid, qPrintable(ordered.at(i)));
        QVERIFY2(Update::compareSemVer(lower, higher) < 0, qPrintable(ordered.at(i) + QStringLiteral(" < ") + ordered.at(i + 1)));
        QVERIFY2(Update::compareSemVer(higher, lower) > 0, qPrintable(ordered.at(i + 1) + QStringLiteral(" > ") + ordered.at(i)));
    }
    // Build metadata does not count.
    QCOMPARE(Update::compareSemVer(Update::parseSemVer(QStringLiteral("1.0.0+build.5")), Update::parseSemVer(QStringLiteral("1.0.0"))), 0);
}

void UpdateTest::semverOfTheChannels()
{
    // The tags of #129 sort in build order; the experimental builds before
    // the release candidates, the release candidates before the release.
    const QStringList ordered = {
        QStringLiteral("v0.2.2-exp.57"), QStringLiteral("v0.2.2-exp.61.pr12"), QStringLiteral("v0.2.2-exp.130"),
        QStringLiteral("v0.2.2-rc.70"), QStringLiteral("v0.2.2-rc.700"), QStringLiteral("v0.2.2"),
        QStringLiteral("v0.2.3-exp.1"),
    };
    for (qsizetype i = 0; i + 1 < ordered.size(); ++i) {
        QVERIFY2(Update::compareSemVer(Update::parseSemVer(ordered.at(i)), Update::parseSemVer(ordered.at(i + 1))) < 0,
                 qPrintable(ordered.at(i) + QStringLiteral(" < ") + ordered.at(i + 1)));
    }
    const Update::SemVer version = Update::parseSemVer(QStringLiteral("v0.2.2-exp.61.pr12"));
    QCOMPARE(Update::semVerText(version), QStringLiteral("0.2.2-exp.61.pr12"));
    QCOMPARE(version.prerelease, (QStringList{QStringLiteral("exp"), QStringLiteral("61"), QStringLiteral("pr12")}));
    QVERIFY(!version.prerelease.isEmpty());
    QVERIFY(Update::parseSemVer(QStringLiteral("0.2.2")).prerelease.isEmpty());
}

void UpdateTest::invalidVersions()
{
    for (const QString& text : {QStringLiteral(""), QStringLiteral("nightly"), QStringLiteral("0.2"),
                                QStringLiteral("0.2.2.1"), QStringLiteral("01.2.3"), QStringLiteral("0.2.2-rc.01"),
                                QStringLiteral("0.2.2-"), QStringLiteral("0.2.2-exp..1"), QStringLiteral("vv0.2.2"),
                                QStringLiteral("99999999999.0.0")}) {
        QVERIFY2(!Update::parseSemVer(text).valid, qPrintable(text));
    }
}

void UpdateTest::listOfReleases()
{
    QString error;
    const QVector<Update::Release> releases = Update::parseReleases(releasesJson(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // "nightly" is not a version: left out. The draft stays in the list
    // (releasesOf() leaves it out).
    QCOMPARE(releases.size(), qsizetype(5));
    const Update::Release& rc = releases.at(1);
    QCOMPARE(rc.tag, QStringLiteral("v0.2.2-rc.70"));
    QVERIFY(rc.prerelease);
    QCOMPARE(rc.published, QDateTime(QDate(2026, 10, 21), QTime(8, 0), QTimeZone::UTC));
    QCOMPARE(rc.assets.size(), qsizetype(1));
    QCOMPARE(rc.assets.first().size, qint64(46000000));
    QCOMPARE(rc.page.host(), QStringLiteral("github.com"));

    QVERIFY(Update::parseReleases("{\"message\": \"Not Found\"}", &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(Update::parseReleases("[{", &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void UpdateTest::releasesOfEachChannel()
{
    const QVector<Update::Release> releases = Update::parseReleases(releasesJson());
    const QString test = QStringLiteral("FinanceDashboardTest");
    const QString prod = QStringLiteral("FinanceDashboard");
    QCOMPARE(versionsOf(Update::releasesOf(releases, Update::Channel::Experimental, test)),
             (QStringList{QStringLiteral("0.2.2-rc.70"), QStringLiteral("0.2.2-exp.61.pr12"), QStringLiteral("0.2.2-exp.57")}));
    QCOMPARE(versionsOf(Update::releasesOf(releases, Update::Channel::Preprod, test)),
             QStringList{QStringLiteral("0.2.2-rc.70")});
    QCOMPARE(versionsOf(Update::releasesOf(releases, Update::Channel::Stable, prod)), QStringList{QStringLiteral("0.2.1")});
    // The packages decide: no Test package in a prod release, and the reverse.
    QVERIFY(Update::releasesOf(releases, Update::Channel::Stable, test).isEmpty());
    QVERIFY(Update::releasesOf(releases, Update::Channel::Experimental, prod).isEmpty());
}

void UpdateTest::controllerFindsTheNewestVersionOfItsChannel()
{
    UpdateController controller;
    controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
    controller.setTestApp(true);
    QCOMPARE(controller.channel(), QStringLiteral("experimental"));
    QCOMPARE(controller.channels().size(), qsizetype(2));
    QVERIFY(controller.checkAtStart()); // on by default

    checkNow(controller);
    QVERIFY2(!controller.statusIsError(), qPrintable(controller.statusText()));
    QCOMPARE(rowVersions(controller.releases()),
             (QStringList{QStringLiteral("0.2.2-rc.70:newer"), QStringLiteral("0.2.2-exp.61.pr12:newer"),
                          QStringLiteral("0.2.2-exp.57:current")}));
    QVERIFY(controller.updateAvailable());
    QCOMPARE(controller.latestVersion(), QStringLiteral("0.2.2-rc.70"));
    QVERIFY(controller.statusText().contains(QStringLiteral("0.2.2-rc.70 is available")));

    // Another channel: the same list, no new download.
    QSignalSpy finished(&controller, &UpdateController::checkFinished);
    controller.setChannel(QStringLiteral("preprod"));
    QCOMPARE(finished.count(), 0);
    QCOMPARE(rowVersions(controller.releases()), QStringList{QStringLiteral("0.2.2-rc.70:newer")});
    QCOMPARE(QSettings().value(QStringLiteral("updates/channel")).toString(), QStringLiteral("preprod"));
    controller.setChannel(QStringLiteral("stable")); // not a channel of the Test app
    QCOMPARE(controller.channel(), QStringLiteral("preprod"));

    controller.setCheckAtStart(false);
    QVERIFY(!controller.checkAtStart());
    QCOMPARE(QSettings().value(QStringLiteral("updates/checkAtStart")).toBool(), false);
}

void UpdateTest::bannerClosedForOneVersionOnly()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.2-exp.61.pr12"));
    {
        UpdateController controller;
        controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
        controller.setTestApp(true);
        checkNow(controller);
        QVERIFY(controller.bannerVisible());
        controller.dismissBanner();
        QVERIFY(!controller.bannerVisible());
        QVERIFY(controller.updateAvailable()); // the page still says it
    }
    {
        // The next start: still closed for rc.70.
        UpdateController controller;
        controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
        controller.setTestApp(true);
        checkNow(controller);
        QVERIFY(controller.updateAvailable());
        QVERIFY(!controller.bannerVisible());
    }
    // A newer version: the banner comes back.
    QByteArray json = releasesJson();
    json.replace("v0.2.2-rc.70", "v0.2.2-rc.75").replace("FinanceDashboardTest-0.2.2-rc.70", "FinanceDashboardTest-0.2.2-rc.75");
    writeList(json);
    UpdateController controller;
    controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
    controller.setTestApp(true);
    checkNow(controller);
    QCOMPARE(controller.latestVersion(), QStringLiteral("0.2.2-rc.75"));
    QVERIFY(controller.bannerVisible());
}

void UpdateTest::prodAppFollowsStable()
{
    QCoreApplication::setApplicationName(QStringLiteral("FinanceDashboard"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.2"));
    UpdateController controller;
    controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
    QCOMPARE(controller.channel(), QStringLiteral("stable"));
    QCOMPARE(controller.channels().size(), qsizetype(1));
    checkNow(controller);
    // v0.2.1 is older than this build: no update, no banner.
    QCOMPARE(rowVersions(controller.releases()), QStringList{QStringLiteral("0.2.1:older")});
    QVERIFY(!controller.updateAvailable());
    QVERIFY(!controller.bannerVisible());
    QVERIFY(controller.statusText().contains(QStringLiteral("up to date")));
    controller.setChannel(QStringLiteral("experimental")); // not for the prod app
    QCOMPARE(controller.channel(), QStringLiteral("stable"));

    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    UpdateController older;
    older.setFeedUrl(QUrl::fromLocalFile(m_listPath));
    checkNow(older);
    QCOMPARE(older.latestVersion(), QStringLiteral("0.2.1"));
}

void UpdateTest::failedCheckKeepsTheApp()
{
    UpdateController controller;
    controller.setTestApp(true);
    controller.setFeedUrl(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("missing.json"))));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not be reached")));
    checkNow(controller);
    QVERIFY(controller.statusIsError());
    QVERIFY(controller.statusText().contains(QStringLiteral("could not be reached")));
    QVERIFY(controller.releases().isEmpty());
    QVERIFY(!controller.updateAvailable());

    writeList("{\"message\": \"API rate limit exceeded\"}");
    controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be read")));
    checkNow(controller);
    QVERIFY(controller.statusIsError());
    QVERIFY(controller.statusText().contains(QStringLiteral("cannot be read")));
    QVERIFY(!controller.listRead());
}

void UpdateTest::bannerStaysClosedForAnOlderVersion()
{
    // The user closed the banner of 0.2.3, then that release went away: the
    // newest version is older than 0.2.3, so no banner.
    QSettings().setValue(QStringLiteral("updates/dismissedVersion"), QStringLiteral("0.2.3"));
    {
        UpdateController controller;
        controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
        controller.setTestApp(true);
        checkNow(controller);
        QCOMPARE(controller.latestVersion(), QStringLiteral("0.2.2-rc.70"));
        QVERIFY(controller.updateAvailable());
        QVERIFY(!controller.bannerVisible());
    }
    // A setting that is not a version does not hide the banner.
    QSettings().setValue(QStringLiteral("updates/dismissedVersion"), QStringLiteral("later"));
    UpdateController controller;
    controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
    controller.setTestApp(true);
    checkNow(controller);
    QVERIFY(controller.bannerVisible());
}

void UpdateTest::checkSendsOnlyTheExpectedRequest()
{
    FakeGitHub github;
    QVERIFY(github.listening());
    // A cookie in the answer: the next check must not send it back.
    github.setAnswer(httpAnswer(200, "OK", releasesJson(), "Set-Cookie: probe=1; Path=/\r\n"));
    UpdateController controller;
    controller.setTestApp(true);
    controller.setFeedUrl(github.feedUrl());
    QVERIFY(!controller.listRead());
    checkNow(controller);
    QVERIFY2(!controller.statusIsError(), qPrintable(controller.statusText()));
    QVERIFY(controller.listRead());
    QCOMPARE(controller.latestVersion(), QStringLiteral("0.2.2-rc.70"));
    checkNow(controller);

    QCOMPARE(github.requests().size(), qsizetype(2));
    for (const QByteArray& head : github.requests()) {
        QCOMPARE(head.left(head.indexOf('\r')),
                 QByteArray("GET /repos/nicolas-goyon/FinanceDashboard/releases?per_page=100 HTTP/1.1"));
        const QHash<QByteArray, QByteArray> headers = headersOf(head);
        QCOMPARE(headers.value("accept"), QByteArray("application/vnd.github+json"));
        QCOMPARE(headers.value("x-github-api-version"), QByteArray("2022-11-28"));
        QCOMPARE(headers.value("user-agent"), QByteArray("FinanceDashboard/0.2.2-exp.57"));
        // Not the language of the computer.
        QCOMPARE(headers.value("accept-language"), QByteArray("en"));
        // Nothing else: no cookie, no token, no other header.
        const QSet<QByteArray> allowed = {"host", "accept", "x-github-api-version", "user-agent",
                                          "accept-language", "accept-encoding", "connection"};
        for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
            QVERIFY2(allowed.contains(it.key()), it.key().constData());
        }
    }
}

void UpdateTest::refusedChecks_data()
{
    QTest::addColumn<QByteArray>("answer");
    QTest::addColumn<QString>("expected"); // a regular expression of the status text

    // The rate limit: the end in x-ratelimit-reset, or the wait in retry-after.
    const qint64 reset = QDateTime::currentSecsSinceEpoch() + 1800;
    const QString resetTime = QDateTime::fromSecsSinceEpoch(reset).toLocalTime().toString(QStringLiteral("HH:mm"));
    QTest::newRow("403, limit with its end")
        << httpAnswer(403, "Forbidden", "{\"message\": \"API rate limit exceeded\"}",
                      "x-ratelimit-remaining: 0\r\nx-ratelimit-reset: " + QByteArray::number(reset) + "\r\n")
        << QStringLiteral("^GitHub refuses more checks until %1: ").arg(resetTime);
    QTest::newRow("403, secondary limit with retry-after")
        << httpAnswer(403, "Forbidden", "{}", "retry-after: 60\r\nx-ratelimit-remaining: 12\r\n")
        << QStringLiteral("^GitHub refuses more checks until \\d\\d:\\d\\d: ");
    QTest::newRow("429, no time") << httpAnswer(429, "Too Many Requests", "{}")
                                  << QStringLiteral("^GitHub refuses more checks for now: ");
    QTest::newRow("403, not a limit") << httpAnswer(403, "Forbidden", "{}")
                                      << QStringLiteral("GitHub answered with the status 403\\.");
    QTest::newRow("500") << httpAnswer(500, "Internal Server Error", "{}")
                         << QStringLiteral("GitHub answered with the status 500\\.");
    QTest::newRow("200, not JSON") << httpAnswer(200, "OK", "<html>")
                                   << QStringLiteral("the answer of GitHub cannot be read");
}

void UpdateTest::refusedChecks()
{
    QFETCH(QByteArray, answer);
    QFETCH(QString, expected);
    FakeGitHub github;
    QVERIFY(github.listening());
    github.setAnswer(answer);
    UpdateController controller;
    controller.setTestApp(true);
    controller.setFeedUrl(github.feedUrl());
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[UpdateController\\] ")));
    checkNow(controller);
    QVERIFY(controller.statusIsError());
    QVERIFY2(controller.statusText().contains(QRegularExpression(expected)), qPrintable(controller.statusText()));
    QVERIFY(controller.releases().isEmpty());
    QVERIFY(!controller.updateAvailable());
}

void UpdateTest::checkWithNoAnswerFails()
{
    FakeGitHub github; // reads the request and never answers
    QVERIFY(github.listening());
    UpdateController controller;
    controller.setTestApp(true);
    controller.setFeedUrl(github.feedUrl());
    controller.setTimeoutForTests(2000);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("did not answer")));
    checkNow(controller);
    QVERIFY(controller.statusIsError());
    QCOMPARE(controller.statusText(),
             QStringLiteral("The check failed: GitHub did not answer within 2 seconds. Try again later."));
    QCOMPARE(github.requests().size(), qsizetype(1));
}

void UpdateTest::startupCheckRunsOnce()
{
    FakeGitHub github;
    QVERIFY(github.listening());
    github.setAnswer(httpAnswer(200, "OK", releasesJson()));
    UpdateController controller;
    controller.setTestApp(true);
    controller.setFeedUrl(github.feedUrl());
    controller.setStartupDelayForTests(50);
    QVERIFY(controller.checkAtStart());
    QSignalSpy finished(&controller, &UpdateController::checkFinished);
    controller.startupCheck();
    controller.startupCheck(); // a second call does nothing
    QVERIFY(!controller.checking()); // the delay first
    QVERIFY(finished.wait(5000));
    QTest::qWait(300);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(github.requests().size(), qsizetype(1));
    QVERIFY(controller.updateAvailable());
}

void UpdateTest::startupCheckTurnedOff()
{
    FakeGitHub github;
    QVERIFY(github.listening());
    github.setAnswer(httpAnswer(200, "OK", releasesJson()));
    {
        // Off in the settings: no check at start.
        UpdateController controller;
        controller.setFeedUrl(github.feedUrl());
        controller.setStartupDelayForTests(50);
        controller.setCheckAtStart(false);
        controller.startupCheck();
        QTest::qWait(300);
        QVERIFY(!controller.checking());
    }
    {
        // Turned off during the delay: no check.
        QSettings().setValue(QStringLiteral("updates/checkAtStart"), true);
        UpdateController controller;
        controller.setFeedUrl(github.feedUrl());
        controller.setStartupDelayForTests(50);
        QVERIFY(controller.checkAtStart());
        controller.startupCheck();
        controller.setCheckAtStart(false);
        QTest::qWait(300);
        QVERIFY(!controller.checking());
    }
    QCOMPARE(github.connections(), 0);
    // "Check now" still works.
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    QVERIFY(!controller.checkAtStart());
    checkNow(controller);
    QCOMPARE(github.requests().size(), qsizetype(1));
}

void UpdateTest::channelChangeDuringACheck()
{
    FakeGitHub github;
    QVERIFY(github.listening());
    github.setAnswer(httpAnswer(200, "OK", releasesJson()));
    UpdateController controller;
    controller.setTestApp(true);
    controller.setFeedUrl(github.feedUrl());
    checkNow(controller);

    // The next check waits for its answer; the user changes the channel.
    github.hold(true);
    QSignalSpy finished(&controller, &UpdateController::checkFinished);
    controller.check();
    QTRY_COMPARE(github.requests().size(), qsizetype(2));
    controller.setChannel(QStringLiteral("preprod"));
    // The versions of the new channel, from the last list; the check goes on.
    QCOMPARE(rowVersions(controller.releases()), QStringList{QStringLiteral("0.2.2-rc.70:newer")});
    QVERIFY(controller.checking());
    QCOMPARE(controller.statusText(), QStringLiteral("Checking…"));
    github.answerNow();
    QVERIFY(finished.wait(5000));
    QVERIFY(controller.statusText().startsWith(QStringLiteral("Version 0.2.2-rc.70 is available.")));
}

void UpdateTest::opensTheProjectPagesOnly_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<bool>("opens");

    QTest::newRow("release page")
        << QStringLiteral("https://github.com/nicolas-goyon/FinanceDashboard/releases/tag/v0.2.1") << true;
    QTest::newRow("project") << QStringLiteral("https://github.com/nicolas-goyon/FinanceDashboard") << true;
    QTest::newRow("other case")
        << QStringLiteral("https://GitHub.com/Nicolas-Goyon/financedashboard/issues/117") << true;
    QTest::newRow("port 443") << QStringLiteral("https://github.com:443/nicolas-goyon/FinanceDashboard/releases")
                              << true;
    QTest::newRow("http") << QStringLiteral("http://github.com/nicolas-goyon/FinanceDashboard/releases") << false;
    QTest::newRow("other repository") << QStringLiteral("https://github.com/someone/FinanceDashboard") << false;
    QTest::newRow("longer name") << QStringLiteral("https://github.com/nicolas-goyon/FinanceDashboardX/releases")
                                 << false;
    QTest::newRow("out with ..")
        << QStringLiteral("https://github.com/nicolas-goyon/FinanceDashboard/../../someone/repository") << false;
    QTest::newRow("other host")
        << QStringLiteral("https://github.com.example.org/nicolas-goyon/FinanceDashboard/releases") << false;
    QTest::newRow("sub-domain") << QStringLiteral("https://gist.github.com/nicolas-goyon/FinanceDashboard") << false;
    QTest::newRow("user name") << QStringLiteral("https://someone@github.com/nicolas-goyon/FinanceDashboard/")
                               << false;
    QTest::newRow("other port") << QStringLiteral("https://github.com:8443/nicolas-goyon/FinanceDashboard/") << false;
    QTest::newRow("file") << QStringLiteral("file:///etc/passwd") << false;
    QTest::newRow("javascript") << QStringLiteral("javascript:alert(1)") << false;
    QTest::newRow("empty") << QString() << false;
    QTest::newRow("encoded slash")
        << QStringLiteral("https://github.com/nicolas-goyon/FinanceDashboard%2F..%2F..%2Fsomeone") << false;
}

void UpdateTest::opensTheProjectPagesOnly()
{
    QFETCH(QString, url);
    QFETCH(bool, opens);
    QCOMPARE(UpdateController::isProjectPage(QUrl(url)), opens);
    if (!opens) {
        // Refused before QDesktopServices: a warning only.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("not opened")));
        UpdateController::openPage(url);
    }
}

void UpdateTest::safeNotes_data()
{
    QTest::addColumn<QString>("notes");
    QTest::addColumn<QString>("removed"); // a text that must be gone, empty for none
    QTest::addColumn<QString>("kept");    // a text that must stay

    QTest::newRow("usual notes") << QStringLiteral("### Added\n\n- **Updates**: the `Updates` page (#117).\n- A "
                                                   "[link](https://github.com/nicolas-goyon/FinanceDashboard).\n\n"
                                                   "| Platform | File |\n|---|---|\n| Linux | `a.AppImage` |\n")
                                 << QString() << QStringLiteral("the `Updates` page");
    QTest::newRow("inline image") << QStringLiteral("Before ![shot](file://server/share/a.png) after.")
                                  << QStringLiteral("server") << QStringLiteral("after");
    QTest::newRow("reference image")
        << QStringLiteral("Before ![shot][s] after.\n\n[s]: https://example.org/b.png")
        << QStringLiteral("example.org") << QStringLiteral("after");
    // Qt inserts HTML of Markdown when its tags close: a lone <img> shows
    // nothing, but <img></img> shows an image.
    QTest::newRow("HTML image") << QStringLiteral("Before <img src=\"https://example.org/c.png\"></img> after.")
                                << QStringLiteral("example.org") << QStringLiteral("after");
    QTest::newRow("image in a table")
        << QStringLiteral("| A | B |\n|---|---|\n| ![x](file:////server/share/d.png) | cell |\n")
        << QStringLiteral("server") << QStringLiteral("cell");
    QTest::newRow("style sheet")
        << QStringLiteral("<link rel=\"stylesheet\" type=\"text/css\" href=\"file://server/share/e.css\"></link>"
                          "\n\nText after.\n")
        << QStringLiteral("server") << QStringLiteral("Text after");
    QTest::newRow("background image")
        << QStringLiteral("<p style=\"background-image:url(file://server/share/f.png)\">Text</p>\n")
        << QStringLiteral("server") << QStringLiteral("Text");
    // HTML written as text: toMarkdown() does not escape all of it. Here the
    // result is still safe (a lone <img> shows nothing)...
    QTest::newRow("HTML as text")
        << QStringLiteral("![x](file://server/share/g.png) &lt;b&gt;&lt;img src=file://server/share/h.png&gt; end")
        << QStringLiteral("g.png") << QStringLiteral("end");
    // ...here toMarkdown() gives "\\<b><img src=...>\\</img></b>", an image
    // again: safeNotes() gives the text only.
    QTest::newRow("HTML as text, closed")
        << QStringLiteral("![x](file://server/share/g.png) &lt;b&gt;&lt;img src=file://server/share/h.png&gt;"
                          "&lt;/img&gt;&lt;/b&gt; end")
        << QStringLiteral("g.png") << QStringLiteral("end");
}

void UpdateTest::safeNotes()
{
    QFETCH(QString, notes);
    QFETCH(QString, removed);
    QFETCH(QString, kept);
    QVERIFY2(readsNoResource(notes) == removed.isEmpty(), "the test data is wrong");
    const QString safe = UpdateController::safeNotes(notes);
    QVERIFY2(readsNoResource(safe), qPrintable(safe));
    QVERIFY2(safe.contains(kept), qPrintable(safe));
    if (removed.isEmpty()) {
        QCOMPARE(safe, notes); // unchanged
    } else {
        QVERIFY2(!safe.contains(removed), qPrintable(safe));
    }
    QCOMPARE(UpdateController::safeNotes(QString()), QString());
}

void UpdateTest::listShowsSafeNotes()
{
    // The notes of rc.70 with an image: the rows of the page have no image.
    QByteArray json = releasesJson();
    json.replace("\"Release candidate.\"", "\"Release candidate.\\n\\n![shot](file://server/share/a.png)\"");
    writeList(json);
    UpdateController controller;
    controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
    controller.setTestApp(true);
    checkNow(controller);
    const QVariantMap rc = controller.releases().first().toMap();
    QCOMPARE(rc.value(QStringLiteral("version")).toString(), QStringLiteral("0.2.2-rc.70"));
    const QString notes = rc.value(QStringLiteral("notes")).toString();
    QVERIFY2(notes.contains(QStringLiteral("Release candidate")), qPrintable(notes));
    QVERIFY2(!notes.contains(QStringLiteral("server")), qPrintable(notes));
    QVERIFY(readsNoResource(notes));
}

void UpdateTest::qmlNetworkReadsLocalFilesOnly()
{
    using Startup::LocalOnlyNetworkAccessManager;
    QVERIFY(LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("file:///tmp/a.png"))));
    QVERIFY(LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("qrc:/qt/qml/FinanceDashboard/Main.qml"))));
    QVERIFY(LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("data:text/plain,hello"))));
    QVERIFY(!LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("file://server/share/a.png"))));
    // No host, but the path of another computer (two separators first).
    QVERIFY(!LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("file:////server/share/a.png"))));
    QVERIFY(!LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("file:///%2Fserver/share/a.png"))));
    QVERIFY(!LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("file:///%5C%5Cserver/share/a.png"))));
    QVERIFY(LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("file:///C:/Users/a.png"))));
    QVERIFY(!LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("http://127.0.0.1/a.png"))));
    QVERIFY(!LocalOnlyNetworkAccessManager::isLocal(QUrl(QStringLiteral("https://example.org/a.png"))));

    FakeGitHub server;
    QVERIFY(server.listening());
    server.setAnswer(httpAnswer(200, "OK", "image"));
    LocalOnlyNetworkAccessManager manager;
    const auto get = [&manager](const QUrl& url) {
        std::unique_ptr<QNetworkReply> reply(manager.get(QNetworkRequest(url)));
        if (!reply->isFinished()) {
            QSignalSpy finished(reply.get(), &QNetworkReply::finished);
            if (!finished.wait(5000)) {
                qWarning() << "no end for" << url;
            }
        }
        return reply;
    };
    // An address of the network: refused at once, with no connection, and a
    // warning gives the address.
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("^\\[LocalOnlyNetwork\\] .*http://127\\.0\\.0\\.1:\\d+/a\\.png$")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[LocalOnlyNetwork\\] .*https://example\\.org/a\\.png$")));
    const auto refused = get(QUrl(QStringLiteral("http://127.0.0.1:%1/a.png").arg(server.port())));
    QCOMPARE(refused->error(), QNetworkReply::ProtocolUnknownError);
    QCOMPARE(get(QUrl(QStringLiteral("https://example.org/a.png")))->error(), QNetworkReply::ProtocolUnknownError);
    QTest::qWait(300);
    QCOMPARE(server.connections(), 0);
    // Local files and data: read.
    const auto data = get(QUrl(QStringLiteral("data:text/plain,hello")));
    QCOMPARE(data->error(), QNetworkReply::NoError);
    QCOMPARE(data->readAll(), QByteArray("hello"));
    const QString path = m_dir.filePath(QStringLiteral("local.txt"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("local");
    }
    const auto local = get(QUrl::fromLocalFile(path));
    QCOMPARE(local->error(), QNetworkReply::NoError);
    QCOMPARE(local->readAll(), QByteArray("local"));

    // The engine uses the factory of main.cpp.
    Startup::LocalOnlyNetworkFactory factory;
    QQmlEngine engine;
    engine.setNetworkAccessManagerFactory(&factory);
    QVERIFY(qobject_cast<LocalOnlyNetworkAccessManager*>(engine.networkAccessManager()) != nullptr);
}

void UpdateTest::markdownImageIsNotDownloaded()
{
    // A Text of UpdatesPage.qml: the notes of a release, in Markdown, with an
    // image on the network (here: the local server). Hidden, as the notes of
    // a closed version.
    FakeGitHub server;
    QVERIFY(server.listening());
    server.setAnswer(httpAnswer(200, "OK", pngImage(), {}, "image/png"));
    const QString notes =
        QStringLiteral("Notes\n\n![screenshot](http://127.0.0.1:%1/screenshot.png)\n").arg(server.port());
    const QByteArray qml = "import QtQuick\nText { visible: false; textFormat: Text.MarkdownText }";

    // Without the factory, the engine downloads the image: the test can see
    // a download.
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(qml, QUrl());
        const std::unique_ptr<QObject> text(component.createWithInitialProperties({{QStringLiteral("text"), notes}}));
        QVERIFY2(text != nullptr, qPrintable(component.errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(server.connections() > 0, 5000);
    }
    const int before = server.connections();
    // With the factory of main.cpp: no connection. Text writes a warning.
    Startup::LocalOnlyNetworkFactory factory;
    QQmlEngine engine;
    engine.setNetworkAccessManagerFactory(&factory);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[LocalOnlyNetwork\\] .*/screenshot\\.png$")));
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("QML Text: Protocol \"refused-by-financedashboard\" is unknown")));
    QQmlComponent component(&engine);
    component.setData(qml, QUrl());
    const std::unique_ptr<QObject> text(component.createWithInitialProperties({{QStringLiteral("text"), notes}}));
    QVERIFY2(text != nullptr, qPrintable(component.errorString()));
    QTest::qWait(1000);
    QCOMPARE(server.connections(), before);
}

void UpdateTest::installKinds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString setupDir = dir.filePath(QStringLiteral("setup"));
    const QString zipDir = dir.filePath(QStringLiteral("zip"));
    QVERIFY(QDir().mkpath(setupDir));
    QVERIFY(QDir().mkpath(zipDir));
    writeFile(setupDir + QStringLiteral("/unins000.exe"), "uninstaller");
    const QString appImage = dir.filePath(QStringLiteral("app.AppImage"));
    writeFile(appImage, "AppImage");

    using Update::InstallKind;
    QCOMPARE(Update::installKindOf(QDir(setupDir), {}, true), InstallKind::WindowsSetup);
    QCOMPARE(Update::installKindOf(QDir(zipDir), {}, true), InstallKind::WindowsPortable);
    QCOMPARE(Update::installKindOf(QDir(zipDir), appImage, false), InstallKind::AppImage);
    // $APPIMAGE of a file that is not there: not an AppImage.
    QCOMPARE(Update::installKindOf(QDir(zipDir), dir.filePath(QStringLiteral("gone.AppImage")), false),
             InstallKind::LinuxArchive);
    QCOMPARE(Update::installKindOf(QDir(QStringLiteral("/opt/FinanceDashboard/bin")), {}, false), InstallKind::LinuxPackage);
    QCOMPARE(Update::installKindOf(QDir(QStringLiteral("/opt/FinanceDashboard/bin/")), {}, false), InstallKind::LinuxPackage);
    QCOMPARE(Update::installKindOf(QDir(zipDir), {}, false), InstallKind::LinuxArchive);

    QVERIFY(Update::installsItself(InstallKind::WindowsSetup));
    QVERIFY(Update::installsItself(InstallKind::AppImage));
    QVERIFY(!Update::installsItself(InstallKind::WindowsPortable));
    QVERIFY(!Update::installsItself(InstallKind::LinuxPackage));
    QVERIFY(!Update::installsItself(InstallKind::LinuxArchive));
    QCOMPARE(Update::installKindName(InstallKind::WindowsSetup), QStringLiteral("setup"));
    QCOMPARE(Update::installKindName(InstallKind::LinuxPackage), QStringLiteral("deb"));

    // The names of the files that CI publishes.
    const QString prod = QStringLiteral("FinanceDashboard");
    QCOMPARE(Update::packageFileName(InstallKind::WindowsSetup, prod, QStringLiteral("0.2.3")),
             QStringLiteral("FinanceDashboard-0.2.3-windows-x64.exe"));
    QCOMPARE(Update::packageFileName(InstallKind::WindowsPortable, prod, QStringLiteral("0.2.3")),
             QStringLiteral("FinanceDashboard-0.2.3-windows-x64.zip"));
    QCOMPARE(Update::packageFileName(InstallKind::AppImage, QStringLiteral("FinanceDashboardTest"),
                                     QStringLiteral("0.2.2-exp.94.pr147")),
             QStringLiteral("FinanceDashboardTest-0.2.2-exp.94.pr147-linux-x86_64.AppImage"));
    QCOMPARE(Update::packageFileName(InstallKind::LinuxPackage, prod, QStringLiteral("0.2.3")),
             QStringLiteral("finance-dashboard_0.2.3_amd64.deb"));
    QCOMPARE(Update::packageFileName(InstallKind::LinuxArchive, prod, QStringLiteral("0.2.3")),
             QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.tar.gz"));
}

void UpdateTest::sumsOfARelease()
{
    const QByteArray lower(64, 'a');
    const QByteArray upper(64, 'B');
    const QByteArray sums = lower + "  FinanceDashboard-0.2.3-windows-x64.exe\r\n" + upper
                            + " *FinanceDashboard-0.2.3-linux-x86_64.AppImage\n" + "not-a-hash  x.zip\n"
                            + QByteArray(63, 'c') + "  short.zip\n";
    QCOMPARE(Update::sha256Of(sums, QStringLiteral("FinanceDashboard-0.2.3-windows-x64.exe")), lower);
    // The "*" of a binary file, and lower case.
    QCOMPARE(Update::sha256Of(sums, QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage")), upper.toLower());
    QVERIFY(Update::sha256Of(sums, QStringLiteral("x.zip")).isEmpty());
    QVERIFY(Update::sha256Of(sums, QStringLiteral("short.zip")).isEmpty());
    // The whole name only.
    QVERIFY(Update::sha256Of(sums, QStringLiteral("FinanceDashboard-0.2.3-windows-x64")).isEmpty());
    QVERIFY(Update::sha256Of({}, QStringLiteral("a.exe")).isEmpty());
}

void UpdateTest::downloadAddresses()
{
    const auto download = [](const char* url) { return UpdateInstaller::isProjectDownload(QUrl(QLatin1String(url))); };
    QVERIFY(download("https://github.com/nicolas-goyon/FinanceDashboard/releases/download/v0.2.1/SHA256SUMS.txt"));
    QVERIFY(download("https://GitHub.com/Nicolas-Goyon/financedashboard/releases/download/v0.2.1/a.exe"));
    QVERIFY(!download("http://github.com/nicolas-goyon/FinanceDashboard/releases/download/v0.2.1/a.exe"));
    QVERIFY(!download("https://github.com/someone/FinanceDashboard/releases/download/v0.2.1/a.exe"));
    QVERIFY(!download("https://github.com/nicolas-goyon/FinanceDashboard/releases/tag/v0.2.1"));
    QVERIFY(!download("https://github.com/nicolas-goyon/FinanceDashboard/releases/download/../../../x/a.exe"));
    QVERIFY(!download("https://example.org/nicolas-goyon/FinanceDashboard/releases/download/v0.2.1/a.exe"));
    QVERIFY(!download("https://someone@github.com/nicolas-goyon/FinanceDashboard/releases/download/v0.2.1/a.exe"));

    const auto host = [](const char* url) { return UpdateInstaller::isGitHubDownloadHost(QUrl(QLatin1String(url))); };
    QVERIFY(host("https://objects.githubusercontent.com/github-production-release-asset-2e65be/1/2"));
    QVERIFY(host("https://release-assets.githubusercontent.com/github-production-release-asset/1/2"));
    QVERIFY(host("https://github.com/nicolas-goyon/FinanceDashboard/releases/download/v0.2.1/a.exe"));
    QVERIFY(!host("http://objects.githubusercontent.com/a"));
    QVERIFY(!host("https://githubusercontent.com.example.org/a"));
    QVERIFY(!host("https://evilgithubusercontent.com/a"));
    QVERIFY(!host("https://someone@objects.githubusercontent.com/a"));
    QVERIFY(!host("https://objects.githubusercontent.com:8443/a"));
}

void UpdateTest::environmentOfTheNewAppImage()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("APPDIR"), QStringLiteral("/tmp/.mount_abc"));
    environment.insert(QStringLiteral("APPIMAGE"), QStringLiteral("/home/u/app.AppImage"));
    environment.insert(QStringLiteral("ARGV0"), QStringLiteral("app.AppImage"));
    environment.insert(QStringLiteral("OWD"), QStringLiteral("/home/u"));
    environment.insert(QStringLiteral("LD_LIBRARY_PATH"), QStringLiteral("/tmp/.mount_abc/usr/lib:/opt/lib"));
    environment.insert(QStringLiteral("QT_PLUGIN_PATH"), QStringLiteral("/tmp/.mount_abc/usr/plugins"));
    environment.insert(QStringLiteral("XDG_DATA_DIRS"), QStringLiteral("/tmp/.mount_abc/usr/share:/usr/local/share:/usr/share"));
    // Another folder whose name starts with the same text: it stays.
    environment.insert(QStringLiteral("QML2_IMPORT_PATH"), QStringLiteral("/tmp/.mount_abcd/usr/qml"));
    environment.insert(QStringLiteral("HOME"), QStringLiteral("/home/u"));
    const QProcessEnvironment result = UpdateInstaller::environmentForNewAppImage(environment);
    for (const char* name : {"APPDIR", "APPIMAGE", "ARGV0", "OWD", "QT_PLUGIN_PATH"}) {
        QVERIFY2(!result.contains(QLatin1String(name)), name);
    }
    QCOMPARE(result.value(QStringLiteral("LD_LIBRARY_PATH")), QStringLiteral("/opt/lib"));
    QCOMPARE(result.value(QStringLiteral("XDG_DATA_DIRS")), QStringLiteral("/usr/local/share:/usr/share"));
    QCOMPARE(result.value(QStringLiteral("QML2_IMPORT_PATH")), QStringLiteral("/tmp/.mount_abcd/usr/qml"));
    QCOMPARE(result.value(QStringLiteral("HOME")), QStringLiteral("/home/u"));
}

void UpdateTest::installReplacesTheAppImage()
{
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage");
    const QByteArray package = packageBytes(300 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("Finance Dashboard.AppImage"));
    writeFile(target, "old");
    QVERIFY(QFile::setPermissions(target, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::AppImage);
    installer.setAppImagePath(target);
    QString launched;
    QStringList launchedArguments;
    QProcessEnvironment launchedEnvironment;
    bool quit = false;
    installer.setLauncherForTests(
        [&](const QString& program, const QStringList& arguments, const QProcessEnvironment& environment) {
            launched = program;
            launchedArguments = arguments;
            launchedEnvironment = environment;
            return true;
        });
    installer.setQuitForTests([&quit]() { quit = true; });

    checkNow(controller);
    QVERIFY(controller.updateAvailable());
    QCOMPARE(controller.installKind(), QStringLiteral("appimage"));
    QVERIFY(controller.installsItself());
    QVERIFY(controller.manualSteps().isEmpty());
    QCOMPARE(controller.installState(), QStringLiteral("idle"));

    QSignalSpy changed(&controller, &UpdateController::installChanged);
    controller.install();
    QCOMPARE(controller.installState(), QStringLiteral("downloading"));
    QVERIFY(controller.bannerVisible());
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("installed"), 10000);
    QCOMPARE(controller.installVersion(), QStringLiteral("0.2.3"));
    QVERIFY(changed.count() >= 2); // downloading, then installed (and the progress between)
    QCOMPARE(controller.installProgress(), 1.0);
    QVERIFY(controller.installText().contains(QStringLiteral("Restart the app")));
    // Cancel and close do nothing now: the new AppImage is in place.
    controller.cancelInstall();
    controller.dismissBanner();
    QCOMPARE(controller.installState(), QStringLiteral("installed"));
    QVERIFY(controller.bannerVisible());
    // The new file, executable, in place of the old one; no file next to it.
    QCOMPARE(readFile(target), package);
    QVERIFY(QFileInfo(target).permissions().testFlag(QFileDevice::ExeOwner));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{QStringLiteral("Finance Dashboard.AppImage")});
    // The list, then SHA256SUMS.txt, then the package; no cookie, English.
    QCOMPARE(github.paths(), (QList<QByteArray>{kFeedPath, "/dl/sums", "/dl/package"}));
    for (const QByteArray& head : github.requests()) {
        const QHash<QByteArray, QByteArray> headers = headersOf(head);
        QVERIFY(!headers.contains("cookie"));
        QCOMPARE(headers.value("accept-language"), QByteArray("en"));
        QCOMPARE(headers.value("user-agent"), QByteArray("FinanceDashboard/0.2.2"));
    }

    // "Restart now": the new AppImage starts without the folders of the old
    // one, then the app quits.
    const QByteArray oldLibraryPath = qgetenv("LD_LIBRARY_PATH");
    const bool hadLibraryPath = qEnvironmentVariableIsSet("LD_LIBRARY_PATH");
    qputenv("APPDIR", "/tmp/.mount_test");
    qputenv("LD_LIBRARY_PATH", "/tmp/.mount_test/usr/lib:/opt/lib");
    controller.restartNow();
    qunsetenv("APPDIR");
    if (hadLibraryPath) {
        qputenv("LD_LIBRARY_PATH", oldLibraryPath);
    } else {
        qunsetenv("LD_LIBRARY_PATH");
    }
    QCOMPARE(launched, QFileInfo(target).absoluteFilePath());
    QVERIFY(launchedArguments.isEmpty());
    QVERIFY(!launchedEnvironment.contains(QStringLiteral("APPDIR")));
    QCOMPARE(launchedEnvironment.value(QStringLiteral("LD_LIBRARY_PATH")), QStringLiteral("/opt/lib"));
    QVERIFY(quit);
}

void UpdateTest::installKeepsTheSetupUntilRestart()
{
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-windows-x64.exe");
    const QByteArray package = packageBytes(120 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString downloads = dir.filePath(QStringLiteral("FinanceDashboard-update"));
    QVERIFY(QDir().mkpath(downloads));
    // A setup and a log of an earlier update: they go.
    writeFile(downloads + QStringLiteral("/FinanceDashboard-0.2.2-windows-x64.exe"), "old setup");
    writeFile(downloads + QStringLiteral("/setup.log"), "old log");

    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::WindowsSetup);
    installer.setDownloadDir(downloads);
    QString launched;
    QStringList launchedArguments;
    bool quit = false;
    installer.setLauncherForTests([&](const QString& program, const QStringList& arguments, const QProcessEnvironment&) {
        launched = program;
        launchedArguments = arguments;
        return true;
    });
    installer.setQuitForTests([&quit]() { quit = true; });

    checkNow(controller);
    QCOMPARE(controller.installKind(), QStringLiteral("setup"));
    controller.install();
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("ready"), 10000);
    QVERIFY(controller.installText().contains(QStringLiteral("closes during the installation")));
    QCOMPARE(QDir(downloads).entryList(QDir::Files), QStringList{name});
    QCOMPARE(readFile(QDir(downloads).filePath(name)), package);
    QVERIFY(launched.isEmpty()); // nothing runs before "Install and restart"
    controller.cancelInstall();
    controller.dismissBanner();
    QCOMPARE(controller.installState(), QStringLiteral("ready"));

    controller.restartNow();
    QCOMPARE(launched, QDir(downloads).filePath(name));
    QStringList expected = UpdateInstaller::setupArguments();
    expected.append(QStringLiteral("/LOG=") + QDir::toNativeSeparators(QDir(downloads).filePath(QStringLiteral("setup.log"))));
    QCOMPARE(launchedArguments, expected);
    QVERIFY(launchedArguments.contains(QStringLiteral("/RESTARTAPP=1")));
    QCOMPARE(controller.installState(), QStringLiteral("installing"));
    QVERIFY(!quit); // the setup closes the app (Restart Manager)
    controller.dismissBanner();
    QCOMPARE(controller.installState(), QStringLiteral("installing"));
    QVERIFY(controller.bannerVisible());
}

void UpdateTest::installRefusesABadDownload_data()
{
    QTest::addColumn<QString>("problem");
    QTest::addColumn<QString>("expected"); // a part of the message

    QTest::newRow("damaged") << QStringLiteral("damaged") << QStringLiteral("is damaged");
    QTest::newRow("bigger") << QStringLiteral("bigger") << QStringLiteral("is bigger than");
    QTest::newRow("incomplete") << QStringLiteral("incomplete") << QStringLiteral("is incomplete");
    QTest::newRow("not listed") << QStringLiteral("not listed") << QStringLiteral("does not list");
    QTest::newRow("no sums") << QStringLiteral("no sums") << QStringLiteral("has no SHA256SUMS.txt");
    QTest::newRow("no package") << QStringLiteral("no package") << QStringLiteral("has no file");
    QTest::newRow("other server") << QStringLiteral("other server") << QStringLiteral("not on the GitHub page");
    QTest::newRow("redirect elsewhere") << QStringLiteral("redirect elsewhere") << QStringLiteral("which is not GitHub");
    QTest::newRow("server error") << QStringLiteral("server error") << QStringLiteral("status 500");
}

void UpdateTest::installRefusesABadDownload()
{
    QFETCH(QString, problem);
    QFETCH(QString, expected);
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage");
    const QByteArray package = packageBytes(64 * 1024);
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(github.port());
    QString packageUrl = base + QStringLiteral("/dl/package");
    qint64 announced = package.size();
    QByteArray sums = sumsFor(sha256Hex(package), name);
    QByteArray packageAnswer = httpAnswer(200, "OK", package, {}, "application/octet-stream");
    QList<std::tuple<QString, QString, qint64>> assets;
    bool withSums = true;
    bool withPackage = true;
    if (problem == QLatin1String("damaged")) {
        sums = sumsFor(sha256Hex("another file"), name);
    } else if (problem == QLatin1String("bigger")) {
        announced = package.size() - 1000;
    } else if (problem == QLatin1String("incomplete")) {
        announced = package.size() + 1000;
    } else if (problem == QLatin1String("not listed")) {
        sums = sumsFor(sha256Hex(package), QStringLiteral("FinanceDashboard-0.2.3-windows-x64.exe"));
    } else if (problem == QLatin1String("no sums")) {
        withSums = false;
    } else if (problem == QLatin1String("no package")) {
        withPackage = false; // the release still has a package of the app: the ZIP
        assets.append({QStringLiteral("FinanceDashboard-0.2.3-windows-x64.zip"), base + QStringLiteral("/dl/zip"), 10});
    } else if (problem == QLatin1String("other server")) {
        packageUrl = QStringLiteral("http://localhost:%1/dl/package").arg(github.port());
    } else if (problem == QLatin1String("redirect elsewhere")) {
        packageAnswer = "HTTP/1.1 302 Found\r\nLocation: http://localhost:" + QByteArray::number(github.port())
                        + "/dl/package\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    } else if (problem == QLatin1String("server error")) {
        packageAnswer = httpAnswer(500, "Internal Server Error", "{}");
    }
    if (withPackage) {
        assets.append({name, packageUrl, announced});
    }
    if (withSums) {
        assets.append({QStringLiteral("SHA256SUMS.txt"), base + QStringLiteral("/dl/sums"), sums.size()});
    }
    github.setRoute(kFeedPath, httpAnswer(200, "OK", releaseJson(QStringLiteral("0.2.3"), assets)));
    github.setRoute("/dl/sums", httpAnswer(200, "OK", sums, {}, "text/plain"));
    github.setRoute("/dl/package", packageAnswer);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("app.AppImage"));
    writeFile(target, "old");
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::AppImage);
    installer.setAppImagePath(target);
    installer.setLauncherForTests([](const QString&, const QStringList&, const QProcessEnvironment&) { return true; });
    installer.setQuitForTests([]() {});
    checkNow(controller);
    QVERIFY(controller.updateAvailable());

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[UpdateInstaller\\] ")));
    controller.install();
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("failed"), 10000);
    QVERIFY2(controller.installText().contains(expected), qPrintable(controller.installText()));
    QVERIFY(controller.bannerVisible());
    // The old AppImage is unchanged, with no file next to it.
    QCOMPARE(readFile(target), QByteArray("old"));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{QStringLiteral("app.AppImage")});
    controller.cancelInstall();
    QCOMPARE(controller.installState(), QStringLiteral("idle"));
}

void UpdateTest::installCanBeCancelled()
{
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage");
    const QByteArray package = packageBytes(64 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));
    github.holdPath("/dl/package");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("app.AppImage"));
    writeFile(target, "old");
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::AppImage);
    installer.setAppImagePath(target);
    checkNow(controller);
    controller.install();
    QTRY_COMPARE(github.paths().size(), qsizetype(3));
    QCOMPARE(controller.installState(), QStringLiteral("downloading"));
    controller.cancelInstall();
    QCOMPARE(controller.installState(), QStringLiteral("idle"));
    // An answer after the cancel changes nothing.
    github.answerNow();
    QTest::qWait(300);
    QCOMPARE(controller.installState(), QStringLiteral("idle"));
    QCOMPARE(readFile(target), QByteArray("old"));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{QStringLiteral("app.AppImage")});
}

void UpdateTest::installFollowsAnAllowedRedirect()
{
    // GitHub sends the file of a release to its storage: the installer
    // follows a redirect to an allowed address (here the same server).
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage");
    const QByteArray package = packageBytes(64 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));
    github.setRoute("/dl/package", "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:" + QByteArray::number(github.port())
                                       + "/storage/abc?signature=1\r\nContent-Length: 5\r\nConnection: close\r\n\r\nmoved");
    github.setRoute("/storage/abc?signature=1", httpAnswer(200, "OK", package, {}, "application/octet-stream"));
    // SHA256SUMS.txt too.
    github.setRoute("/dl/sums", "HTTP/1.1 302 Found\r\nLocation: /storage/sums\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    github.setRoute("/storage/sums", httpAnswer(200, "OK", sumsFor(sha256Hex(package), name), {}, "text/plain"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("app.AppImage"));
    writeFile(target, "old");
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::AppImage);
    installer.setAppImagePath(target);
    checkNow(controller);
    controller.install();
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("installed"), 10000);
    // The body of the redirect ("moved") is not in the file.
    QCOMPARE(readFile(target), package);
    QCOMPARE(github.paths(), (QList<QByteArray>{kFeedPath, "/dl/sums", "/storage/sums", "/dl/package",
                                                "/storage/abc?signature=1"}));
}

void UpdateTest::installAgainAfterAFailure()
{
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage");
    const QByteArray package = packageBytes(64 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));
    github.setRoute("/dl/package", httpAnswer(500, "Internal Server Error", "{}"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("app.AppImage"));
    writeFile(target, "old");
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::AppImage);
    installer.setAppImagePath(target);
    checkNow(controller);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[UpdateInstaller\\] ")));
    controller.install();
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("failed"), 10000);
    // Closing the banner ends the failure, and closes the banner of 0.2.3.
    controller.dismissBanner();
    QCOMPARE(controller.installState(), QStringLiteral("idle"));
    QVERIFY(!controller.bannerVisible());

    // The server works again: "Install" on the Updates page.
    github.setRoute("/dl/package", httpAnswer(200, "OK", package, {}, "application/octet-stream"));
    controller.install();
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("installed"), 10000);
    QCOMPARE(readFile(target), package);
}

void UpdateTest::installStopsADownloadWithNoData()
{
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage");
    const QByteArray package = packageBytes(64 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));
    github.holdPath("/dl/package"); // never answers

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("app.AppImage"));
    writeFile(target, "old");
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::AppImage);
    installer.setAppImagePath(target);
    installer.setTimeoutForTests(2000);
    checkNow(controller);
    // After the timeout Qt closes the reply: nothing reads it any more.
    QTest::failOnWarning(QRegularExpression(QStringLiteral("device not open")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[UpdateInstaller\\] ")));
    controller.install();
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("failed"), 10000);
    QVERIFY2(controller.installText().contains(QStringLiteral("no data came for 2 seconds")),
             qPrintable(controller.installText()));
    QCOMPARE(readFile(target), QByteArray("old"));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{QStringLiteral("app.AppImage")});
}

void UpdateTest::installChecksTheSetupAgainBeforeItRuns()
{
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-windows-x64.exe");
    const QByteArray package = packageBytes(64 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString downloads = dir.filePath(QStringLiteral("FinanceDashboard-update"));
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::WindowsSetup);
    installer.setDownloadDir(downloads);
    bool launched = false;
    installer.setLauncherForTests([&launched](const QString&, const QStringList&, const QProcessEnvironment&) {
        launched = true;
        return true;
    });
    checkNow(controller);
    controller.install();
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("ready"), 10000);
    // Something changes the setup while it waits.
    QByteArray changed = package;
    changed[100] = static_cast<char>(changed.at(100) ^ 0x55);
    writeFile(QDir(downloads).filePath(name), changed);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[UpdateInstaller\\] ")));
    controller.restartNow();
    QVERIFY(!launched);
    QCOMPARE(controller.installState(), QStringLiteral("failed"));
    QVERIFY2(controller.installText().contains(QStringLiteral("changed after the download")),
             qPrintable(controller.installText()));
}

void UpdateTest::installWaitsForTheSetupThenOffersAgain()
{
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-windows-x64.exe");
    const QByteArray package = packageBytes(16 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::WindowsSetup);
    installer.setDownloadDir(dir.filePath(QStringLiteral("FinanceDashboard-update")));
    installer.setLauncherForTests([](const QString&, const QStringList&, const QProcessEnvironment&) { return true; });
    installer.setInstallWaitForTests(200);
    checkNow(controller);
    controller.install();
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("ready"), 10000);
    controller.restartNow();
    QCOMPARE(controller.installState(), QStringLiteral("installing"));
    // The setup did not close the app (a refused administrator prompt).
    QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), QStringLiteral("ready"), 5000);
    QVERIFY2(controller.installText().contains(QStringLiteral("has not closed the app yet")),
             qPrintable(controller.installText()));
}

void UpdateTest::installTellsWhenTheNewVersionDoesNotStart()
{
    useProdApp();
    for (const auto kind : {Update::InstallKind::WindowsSetup, Update::InstallKind::AppImage}) {
        FakeGitHub github;
        QVERIFY(github.listening());
        const QString name = Update::packageFileName(kind, QStringLiteral("FinanceDashboard"), QStringLiteral("0.2.3"));
        const QByteArray package = packageBytes(16 * 1024);
        serveRelease(github, name, package, sumsFor(sha256Hex(package), name));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString target = dir.filePath(QStringLiteral("app.AppImage"));
        writeFile(target, "old");
        UpdateController controller;
        controller.setFeedUrl(github.feedUrl());
        UpdateInstaller& installer = controller.installerForTests();
        installer.setKind(kind);
        installer.setAppImagePath(target);
        installer.setDownloadDir(dir.filePath(QStringLiteral("FinanceDashboard-update")));
        bool quit = false;
        installer.setLauncherForTests([](const QString&, const QStringList&, const QProcessEnvironment&) { return false; });
        installer.setQuitForTests([&quit]() { quit = true; });
        checkNow(controller);
        controller.install();
        const QString waiting = kind == Update::InstallKind::AppImage ? QStringLiteral("installed") : QStringLiteral("ready");
        QTRY_COMPARE_WITH_TIMEOUT(controller.installState(), waiting, 10000);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[UpdateInstaller\\] ")));
        controller.restartNow();
        QCOMPARE(controller.installState(), QStringLiteral("failed"));
        QVERIFY2(controller.installText().contains(QStringLiteral("did not start")), qPrintable(controller.installText()));
        QVERIFY(!quit); // the app stays open
    }
}

void UpdateTest::installNeedsAWritableAppImage()
{
    useProdApp();
    FakeGitHub github;
    QVERIFY(github.listening());
    const QString name = QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage");
    const QByteArray package = packageBytes(16 * 1024);
    serveRelease(github, name, package, sumsFor(sha256Hex(package), name));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    UpdateController controller;
    controller.setFeedUrl(github.feedUrl());
    UpdateInstaller& installer = controller.installerForTests();
    installer.setKind(Update::InstallKind::AppImage);
    // A folder that does not exist: the app cannot write there (as with a
    // folder of root, for a user).
    installer.setAppImagePath(dir.filePath(QStringLiteral("gone/app.AppImage")));
    checkNow(controller);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^\\[UpdateInstaller\\] ")));
    controller.install();
    QCOMPARE(controller.installState(), QStringLiteral("failed"));
    QVERIFY2(controller.installText().contains(QStringLiteral("replace the file by hand")),
             qPrintable(controller.installText()));
    // It failed before any download.
    QCOMPARE(github.paths(), QList<QByteArray>{kFeedPath});
}

void UpdateTest::installAllowsTheFeedServerOnlyOnThisComputer()
{
    // The list of GitHub is on api.github.com: a file there is not a file of
    // a release, so the installer does not download it.
    QNetworkAccessManager network;
    UpdateInstaller installer(&network);
    installer.setKind(Update::InstallKind::AppImage);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("app.AppImage"));
    writeFile(target, "old");
    installer.setAppImagePath(target);
    installer.setAllowedOrigin(QUrl(QString::fromLatin1(UpdateController::kDefaultFeedUrl)));
    Update::Release release;
    release.assets = {{QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage"),
                       QUrl(QStringLiteral("https://api.github.com/x.AppImage")), 10},
                      {QStringLiteral("SHA256SUMS.txt"), QUrl(QStringLiteral("https://api.github.com/sums")), 10}};
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("not on the GitHub page")));
    installer.start(release, QStringLiteral("FinanceDashboard-0.2.3-linux-x86_64.AppImage"), QStringLiteral("0.2.3"));
    QCOMPARE(installer.state(), UpdateInstaller::State::Failed);
}

void UpdateTest::oldDownloadsGo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto fill = [&dir]() {
        writeFile(dir.filePath(QStringLiteral("FinanceDashboard-0.2.2-windows-x64.exe")), "setup");
        writeFile(dir.filePath(QStringLiteral("FinanceDashboard-0.2.3-windows-x64.exe.Ab12Cd")), "part");
        writeFile(dir.filePath(QStringLiteral("setup.log")), "log");
    };
    fill();
    // At start: the log of a failed update stays.
    UpdateInstaller::removeOldDownloads(dir.path(), true);
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{QStringLiteral("setup.log")});
    fill();
    // Before a new download: everything goes.
    UpdateInstaller::removeOldDownloads(dir.path(), false);
    QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
}

void UpdateTest::manualStepsForTheOtherCopies_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<bool>("testApp");
    QTest::addColumn<QString>("expected");

    QTest::newRow("portable ZIP") << int(Update::InstallKind::WindowsPortable) << false
                                  << QStringLiteral("Download FinanceDashboard-0.2.3-windows-x64.zip");
    QTest::newRow(".deb") << int(Update::InstallKind::LinuxPackage) << false
                          << QStringLiteral("sudo apt install ./finance-dashboard_0.2.3_amd64.deb");
    QTest::newRow(".tar.gz") << int(Update::InstallKind::LinuxArchive) << false
                             << QStringLiteral("Download FinanceDashboard-0.2.3-linux-x86_64.tar.gz");
    QTest::newRow("Test app, not an AppImage") << int(Update::InstallKind::LinuxArchive) << true
                                               << QStringLiteral("Download FinanceDashboardTest-0.2.2-rc.70-linux-x86_64.AppImage");
}

void UpdateTest::manualStepsForTheOtherCopies()
{
    QFETCH(int, kind);
    QFETCH(bool, testApp);
    QFETCH(QString, expected);
    if (!testApp) {
        useProdApp();
        writeList(releaseJson(QStringLiteral("0.2.3"),
                              {{QStringLiteral("FinanceDashboard-0.2.3-windows-x64.zip"),
                                QStringLiteral("https://github.com/nicolas-goyon/FinanceDashboard/releases/download/v0.2.3/a.zip"),
                                10}}));
    }
    UpdateController controller;
    controller.setFeedUrl(QUrl::fromLocalFile(m_listPath));
    controller.setTestApp(testApp);
    controller.installerForTests().setKind(static_cast<Update::InstallKind>(kind));
    checkNow(controller);
    QVERIFY(controller.updateAvailable());
    QVERIFY(!controller.installsItself());
    QVERIFY2(controller.manualSteps().contains(expected), qPrintable(controller.manualSteps()));
    controller.install(); // nothing for a copy that updates by hand
    QCOMPARE(controller.installState(), QStringLiteral("idle"));
}

// A QGuiApplication: the QML Text of markdownImageIsNotDownloaded().
QTEST_MAIN(UpdateTest)
#include "update_test.moc"
