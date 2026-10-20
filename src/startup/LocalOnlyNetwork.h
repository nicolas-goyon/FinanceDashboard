#ifndef FINANCEDASHBOARD_STARTUP_LOCALONLYNETWORK_H
#define FINANCEDASHBOARD_STARTUP_LOCALONLYNETWORK_H

#include <QNetworkAccessManager>
#include <QQmlNetworkAccessManagerFactory>
#include <QUrl>

class QIODevice;
class QNetworkReply;
class QNetworkRequest;
class QObject;

// ── Startup::LocalOnlyNetworkAccessManager / LocalOnlyNetworkFactory ─────────
//
// The network access of the QML engine (#117). The app promises that the
// update check and the download of a new version (UpdateController and
// UpdateInstaller, with their own QNetworkAccessManager) are its only network
// calls (ARCHITECTURE.MD § Network rule). But QML loads remote
// files by itself: an image in the Markdown of release notes, an Image with an
// http source. main.cpp gives the engine this factory, so the network access
// of the QML engine reads local files and resources only: every other address
// (http, https, a file of another computer) fails at once, with no connection,
// and a warning gives the address.
//
// QML opens a file: address itself, without the network access: this factory
// does not see it. On Windows, file://server/share/... is a file of another
// computer (a network share), so untrusted text must not reach a rich Text:
// UpdateController removes the images from the release notes first
// (UpdateController::safeNotes()).
//
// https://doc.qt.io/qt-6/qqmlengine.html#setNetworkAccessManagerFactory

namespace Startup {

class LocalOnlyNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    explicit LocalOnlyNetworkAccessManager(QObject* parent = nullptr);

    // True for the addresses that this manager reads: a file of this computer
    // (file: with no host, and not a //server path), a resource (qrc:) and
    // data in the address (data:).
    [[nodiscard]] static bool isLocal(const QUrl& url);

protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* outgoingData) override;
};

class LocalOnlyNetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager* create(QObject* parent) override;
};

} // namespace Startup

#endif // FINANCEDASHBOARD_STARTUP_LOCALONLYNETWORK_H
