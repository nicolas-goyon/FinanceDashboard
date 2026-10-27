#include "startup/LocalOnlyNetwork.h"

#include <QDebug>
#include <QIODevice>
#include <QLatin1Char>
#include <QLatin1String>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QtLogging>

namespace Startup {

LocalOnlyNetworkAccessManager::LocalOnlyNetworkAccessManager(QObject* parent)
    : QNetworkAccessManager(parent)
{}

bool LocalOnlyNetworkAccessManager::isLocal(const QUrl& url)
{
    // A file URL with a host (file://server/share/...) is a file of another
    // computer: on Windows, a UNC path. file:////server/share and
    // file:///%2Fserver/share have no host, but give the same path: two
    // separators first ("/" or "\\", both separators on Windows). QUrl keeps
    // the scheme in lower case.
    if (url.isLocalFile()) {
        const QString path = url.toLocalFile().replace(QLatin1Char('\\'), QLatin1Char('/'));
        return url.host().isEmpty() && !path.startsWith(QLatin1String("//"));
    }
    const QString scheme = url.scheme();
    return scheme == QLatin1String("qrc") || scheme == QLatin1String("data");
}

QNetworkReply* LocalOnlyNetworkAccessManager::createRequest(Operation operation, const QNetworkRequest& request,
                                                            QIODevice* outgoingData)
{
    if (isLocal(request.url())) {
        return QNetworkAccessManager::createRequest(operation, request, outgoingData);
    }
    // Give Qt a scheme that it does not know: the reply fails at once with
    // QNetworkReply::ProtocolUnknownError, before any connection or name
    // lookup. The warning of QML gives only that scheme, so write the address
    // here (without a user name or a password).
    qWarning().noquote() << "[LocalOnlyNetwork] no network for QML, not loaded:"
                         << request.url().toString(QUrl::RemoveUserInfo);
    QNetworkRequest refused(request);
    refused.setUrl(QUrl(QStringLiteral("refused-by-financedashboard:")));
    return QNetworkAccessManager::createRequest(operation, refused, outgoingData);
}

QNetworkAccessManager* LocalOnlyNetworkFactory::create(QObject* parent)
{
    // Called by the QML engine, possibly from its loader threads: no shared
    // state here.
    return new LocalOnlyNetworkAccessManager(parent);
}

} // namespace Startup
