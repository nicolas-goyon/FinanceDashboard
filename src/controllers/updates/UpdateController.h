#ifndef FINANCEDASHBOARD_CONTROLLERS_UPDATES_UPDATECONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_UPDATES_UPDATECONTROLLER_H

#include "update/ReleaseFeed.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>
#include <QtQml/qqml.h>

class QNetworkReply;

// ── UpdateController ─────────────────────────────────────────────────────────
//
// Backend of qml/pages/UpdatesPage.qml and of the update banner of Main.qml
// (#117, first part: the check). A QML_SINGLETON: the page and the banner
// show the same state.
//
// The check reads the releases of the project on GitHub (one GET of the REST
// API, no token: the repository is public), keeps the releases of the channel
// of this app (Update::releasesOf()) and compares them with the version of the
// app (QCoreApplication::applicationVersion(), FD_VERSION_STRING). The prod
// app follows the Stable channel; the Test app (#130) follows Experimental
// (the -exp and -rc pre-releases) or Preprod (-rc only), as the user chooses.
//
// The app checks at each start, unless the user turned it off (decision of
// 2026-10-20). GitHub sees the internet address of the computer and the
// version of the app (in the User-Agent header); the app sends nothing else:
// no cookie, no token, and "en" as the language, not the language of the
// computer. Without a network, when GitHub does not answer in time, or when
// GitHub refuses the call (60 calls an hour for each address without a token:
// 403 or 429), the status tells why and the app continues to work.
//
// The settings (QSettings, so each app identity has its own): updates/
// checkAtStart, updates/channel, and updates/dismissedVersion (the version
// whose banner the user closed: the banner comes back for a newer one).
//
// The notes of a release come from GitHub (and can include the title of a
// pull request): safeNotes() removes their images, and anything else that
// makes a Text read a file or an address, before QML shows them (on Windows,
// file://server/... is a network share).
//
// The download and the installation come in the second part of #117: for
// now, the page opens the release page of a version. openPage() opens the
// pages of this project on GitHub only (isProjectPage()).

class UpdateController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Set by Main.qml (from main.cpp) before the check at start.
    Q_PROPERTY(bool testApp READ testApp WRITE setTestApp NOTIFY settingsChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool checkAtStart READ checkAtStart WRITE setCheckAtStart NOTIFY settingsChanged)
    // "stable" in the prod app; "experimental" or "preprod" in the Test app.
    Q_PROPERTY(QString channel READ channel WRITE setChannel NOTIFY settingsChanged)
    // The channels this app can follow: { id, label } maps.
    Q_PROPERTY(QVariantList channels READ channels NOTIFY settingsChanged)
    // The address of the list of releases. Tests set a file:// address.
    Q_PROPERTY(QUrl feedUrl READ feedUrl WRITE setFeedUrl NOTIFY settingsChanged)

    Q_PROPERTY(bool checking READ checking NOTIFY stateChanged)
    // A check read the list at least once (before, an empty list of releases
    // does not mean "no version in this channel").
    Q_PROPERTY(bool listRead READ listRead NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY stateChanged)
    // The releases of the channel, newest first: { tag, version, title, date,
    // notes, page, relation ("newer", "current" or "older") } maps.
    Q_PROPERTY(QVariantList releases READ releases NOTIFY stateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
    // updateAvailable, unless the user closed the banner of that version.
    Q_PROPERTY(bool bannerVisible READ bannerVisible NOTIFY stateChanged)

public:
    explicit UpdateController(QObject* parent = nullptr);

    // https://docs.github.com/en/rest/releases/releases#list-releases
    static constexpr const char* kDefaultFeedUrl =
        "https://api.github.com/repos/nicolas-goyon/FinanceDashboard/releases?per_page=100";
    // The pages that openPage() opens: this project on GitHub.
    static constexpr const char* kProjectPagePath = "/nicolas-goyon/FinanceDashboard";
    static constexpr int kStartupDelayMs = 3000; // the check at start waits for the window
    static constexpr int kTimeoutMs = 20000;     // a check that gets no data for 20 s fails

    [[nodiscard]] bool testApp() const { return m_testApp; }
    void setTestApp(bool testApp);
    [[nodiscard]] QString currentVersion() const { return m_currentVersion; }
    [[nodiscard]] bool checkAtStart() const { return m_checkAtStart; }
    void setCheckAtStart(bool check);
    [[nodiscard]] QString channel() const;
    void setChannel(const QString& channel);
    [[nodiscard]] QVariantList channels() const;
    [[nodiscard]] QUrl feedUrl() const { return m_feedUrl; }
    void setFeedUrl(const QUrl& url);

    [[nodiscard]] bool checking() const { return m_reply != nullptr; }
    [[nodiscard]] bool listRead() const { return m_listRead; }
    [[nodiscard]] QString statusText() const { return m_statusText; }
    [[nodiscard]] bool statusIsError() const { return m_statusIsError; }
    [[nodiscard]] QVariantList releases() const { return m_releaseRows; }
    [[nodiscard]] bool updateAvailable() const { return !m_latestNewer.isEmpty(); }
    [[nodiscard]] QString latestVersion() const { return m_latestNewer; }
    [[nodiscard]] bool bannerVisible() const;

    // Reads the list of releases now. Does nothing while a check runs.
    Q_INVOKABLE void check();
    // The check at start: once for each run of the app, a few seconds after
    // the window opens, and only when checkAtStart is true.
    Q_INVOKABLE void startupCheck();
    // Opens `url` (the page of a release, a link of its notes) in the
    // browser, when isProjectPage(url). Else it writes a warning only.
    Q_INVOKABLE static void openPage(const QString& url);
    // True for https://github.com/nicolas-goyon/FinanceDashboard and the
    // pages below it (GitHub ignores the case of these two names).
    [[nodiscard]] static bool isProjectPage(const QUrl& url);
    // `markdown` (the notes of a release) as Markdown that Text.MarkdownText
    // shows without reading a file or an address: no image (inline,
    // reference or HTML), no background image, no style sheet. Usually the
    // same text; else the text without them, or as plain text at worst. It
    // reads no resource itself.
    [[nodiscard]] static QString safeNotes(const QString& markdown);
    // Hides the banner until a newer version than latestVersion exists.
    Q_INVOKABLE void dismissBanner();

    // The tests only: shorter times than kStartupDelayMs and kTimeoutMs.
    void setStartupDelayForTests(int milliseconds) { m_startupDelayMs = milliseconds; }
    void setTimeoutForTests(int milliseconds) { m_timeoutMs = milliseconds; }

signals:
    void settingsChanged();
    void stateChanged();
    // After each check, also a failed one.
    void checkFinished();

private:
    void finish(QNetworkReply* reply);
    void fail(const QString& message);
    // Keeps the releases of the channel, and the newest one when it is newer
    // than this app.
    void applyChannel();
    [[nodiscard]] Update::Channel currentChannel() const;

    QNetworkAccessManager m_network;
    QNetworkReply* m_reply = nullptr;
    QUrl m_feedUrl;
    int m_startupDelayMs = kStartupDelayMs;
    int m_timeoutMs = kTimeoutMs;
    QString m_currentVersion;  // QCoreApplication::applicationVersion()
    bool m_checkAtStart = true; // the setting updates/checkAtStart
    bool m_testApp = false;
    bool m_startupCheckDone = false;
    bool m_listRead = false;               // a check read the list at least once
    QVector<Update::Release> m_allReleases; // the last list, every channel
    QVariantList m_releaseRows;
    QString m_latestNewer;                 // the newest version of the channel, when newer
    QDateTime m_checkedAt;                 // the time of the last check that read the list
    QString m_statusText;
    bool m_statusIsError = false;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_UPDATES_UPDATECONTROLLER_H
