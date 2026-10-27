#include "update/ReleaseFeed.h"
#include "update/SemVer.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLatin1Char>
#include <QLatin1String>
#include <QList>
#include <QString>
#include <QUrl>
#include <QVector>
#include <Qt>
#include <QtTypes>

#include <algorithm>

namespace Update {

QVector<Release> parseReleases(const QByteArray& json, QString* error)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error != nullptr) {
            *error = parseError.error != QJsonParseError::NoError
                         ? parseError.errorString()
                         : QStringLiteral("the answer is not a list of releases");
        }
        return {};
    }
    QVector<Release> releases;
    const QJsonArray list = document.array();
    releases.reserve(list.size());
    for (const auto& value : list) {
        const QJsonObject object = value.toObject();
        Release release;
        release.tag = object.value(QLatin1String("tag_name")).toString();
        release.version = parseSemVer(release.tag);
        if (!release.version.valid) {
            continue;
        }
        release.title = object.value(QLatin1String("name")).toString();
        release.notes = object.value(QLatin1String("body")).toString();
        release.page = QUrl(object.value(QLatin1String("html_url")).toString());
        release.published = QDateTime::fromString(object.value(QLatin1String("published_at")).toString(), Qt::ISODate);
        release.prerelease = object.value(QLatin1String("prerelease")).toBool();
        release.draft = object.value(QLatin1String("draft")).toBool();
        const QJsonArray assets = object.value(QLatin1String("assets")).toArray();
        for (const auto& assetValue : assets) {
            const QJsonObject asset = assetValue.toObject();
            release.assets.append({asset.value(QLatin1String("name")).toString(),
                                   QUrl(asset.value(QLatin1String("browser_download_url")).toString()),
                                   static_cast<qint64>(asset.value(QLatin1String("size")).toDouble())});
        }
        releases.append(release);
    }
    if (error != nullptr) {
        error->clear();
    }
    return releases;
}

namespace {

bool inChannel(const Release& release, Channel channel)
{
    const QString kind = release.version.prerelease.value(0);
    switch (channel) {
    case Channel::Stable:
        return !release.prerelease && release.version.prerelease.isEmpty();
    case Channel::Experimental:
        return release.prerelease && (kind == QLatin1String("exp") || kind == QLatin1String("rc"));
    case Channel::Preprod:
        return release.prerelease && kind == QLatin1String("rc");
    }
    return false;
}

bool hasPackageOf(const Release& release, const QString& appName)
{
    const QString prefix = appName + QLatin1Char('-');
    return std::any_of(release.assets.cbegin(), release.assets.cend(),
                       [&prefix](const ReleaseAsset& asset) { return asset.name.startsWith(prefix); });
}

} // namespace

QVector<Release> releasesOf(const QVector<Release>& releases, Channel channel, const QString& appName)
{
    QVector<Release> result;
    for (const Release& release : releases) {
        if (!release.draft && inChannel(release, channel) && hasPackageOf(release, appName)) {
            result.append(release);
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const Release& left, const Release& right) {
        return compareSemVer(left.version, right.version) > 0;
    });
    return result;
}

} // namespace Update
