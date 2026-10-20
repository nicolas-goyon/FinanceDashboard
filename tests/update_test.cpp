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
//   - the network access of the QML engine (Startup::LocalOnlyNetworkFactory):
//     local files only, and an image in Markdown notes is not downloaded.

#include "UpdateController.h"
#include "startup/LocalOnlyNetwork.h"
#include "update/ReleaseFeed.h"
#include "update/SemVer.h"

#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
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
    void hold(bool hold) { m_hold = hold; }
    void answerNow()
    {
        const QList<QPointer<QTcpSocket>> held = std::exchange(m_held, {});
        for (const QPointer<QTcpSocket>& socket : held) {
            if (socket != nullptr) {
                answer(socket);
            }
        }
    }
    [[nodiscard]] int connections() const { return m_connections; }
    [[nodiscard]] const QList<QByteArray>& requests() const { return m_requests; }

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
        m_requests.append(data.left(end));
        m_received.remove(socket);
        if (m_hold) {
            m_held.append(socket);
            return;
        }
        answer(socket);
    }
    void answer(QTcpSocket* socket)
    {
        if (m_answer.isEmpty()) {
            return;
        }
        socket->write(m_answer);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    bool m_listening = false;
    QByteArray m_answer;
    bool m_hold = false;
    int m_connections = 0;
    QHash<QTcpSocket*, QByteArray> m_received;
    QList<QByteArray> m_requests;
    QList<QPointer<QTcpSocket>> m_held;
};

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

// A QGuiApplication: the QML Text of markdownImageIsNotDownloaded().
QTEST_MAIN(UpdateTest)
#include "update_test.moc"
