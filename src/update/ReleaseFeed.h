#ifndef FINANCEDASHBOARD_UPDATE_RELEASEFEED_H
#define FINANCEDASHBOARD_UPDATE_RELEASEFEED_H

#include "update/SemVer.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <QVector>

#include <cstdint>

// ── Update::ReleaseFeed ──────────────────────────────────────────────────────
//
// The releases of the project on GitHub, as the REST API lists them
// (GET /repos/{owner}/{repo}/releases, https://docs.github.com/en/rest/releases/releases),
// and the releases of one channel (#129, #117):
//
//   - Stable: the releases (not pre-releases) — the prod app;
//   - Experimental: the -exp and -rc pre-releases — the Test app;
//   - Preprod: the -rc pre-releases only — the Test app.
//
// A release counts only when it has a package of the app that asks: a file
// whose name starts with "<application name>-" (FinanceDashboard-… for the
// prod app, FinanceDashboardTest-… for the Test app). The releases of the
// prod app have no Test packages, and the pre-releases no prod packages.
//
// No network here: UpdateController downloads the list and gives the JSON.

namespace Update {

struct ReleaseAsset
{
    QString name;
    QUrl    downloadUrl; // browser_download_url
    qint64  size = 0;
};

struct Release
{
    QString  tag;        // "v0.2.2-exp.57"
    SemVer   version;    // from the tag
    QString  title;      // the name of the release
    QString  notes;      // its body (Markdown)
    QUrl     page;       // html_url: the page of the release
    QDateTime published; // published_at
    bool     prerelease = false;
    bool     draft = false;
    QVector<ReleaseAsset> assets;
};

enum class Channel : std::uint8_t { Stable, Experimental, Preprod };

// Every release of a JSON list. A release whose tag is not a version is left
// out. On a JSON error, `error` (when not null) gets the message and the list
// is empty.
QVector<Release> parseReleases(const QByteArray& json, QString* error = nullptr);

// The releases of `channel` that have a package of `appName`, newest first
// (SemVer order). Drafts never count.
QVector<Release> releasesOf(const QVector<Release>& releases, Channel channel, const QString& appName);

} // namespace Update

#endif // FINANCEDASHBOARD_UPDATE_RELEASEFEED_H
