#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── ImportController ──────────────────────────────────────────────────────────
//
// C++ backend for qml/tabs/ImportTab.qml.
// Wraps FileFinder + JsonReader<Data::PortfolioSummary>; exposes the parsed
// result pre-formatted for display.

class ImportController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString statusText  READ statusText  NOTIFY resultChanged)
    // "idle" | "ok" | "error"
    Q_PROPERTY(QString statusState READ statusState NOTIFY resultChanged)
    Q_PROPERTY(bool    hasResult   READ hasResult   NOTIFY resultChanged)
    // Message shown in the results area when there is no result (hint/error)
    Q_PROPERTY(QString message     READ message     NOTIFY resultChanged)

    // [{ key, value, color }] — summary card rows
    Q_PROPERTY(QVariantList summaryRows READ summaryRows NOTIFY resultChanged)
    // [{ time, value }] — capped preview of the time series (values formatted)
    Q_PROPERTY(QVariantList points      READ points      NOTIFY resultChanged)
    Q_PROPERTY(int totalPoints  READ totalPoints  NOTIFY resultChanged)
    Q_PROPERTY(QString capNote  READ capNote      NOTIFY resultChanged)

public:
    explicit ImportController(QObject* parent = nullptr);

    QString statusText()  const { return m_statusText; }
    QString statusState() const { return m_statusState; }
    bool    hasResult()   const { return m_hasResult; }
    QString message()     const { return m_message; }

    QVariantList summaryRows() const { return m_summaryRows; }
    QVariantList points()      const { return m_points; }
    int     totalPoints()      const { return m_totalPoints; }
    QString capNote()          const { return m_capNote; }

    Q_INVOKABLE void importFile(const QString& path);
    // file:/// url → local path (for FileDialog.selectedFile)
    Q_INVOKABLE QString localPath(const QUrl& url) const;

signals:
    void resultChanged();

private:
    void setFailure(const QString& status, const QString& message);

    QString      m_statusText  = QStringLiteral("No file imported yet.");
    QString      m_statusState = QStringLiteral("idle");
    bool         m_hasResult   = false;
    QString      m_message     = QStringLiteral("Import a JSON file to see results here.");
    QVariantList m_summaryRows;
    QVariantList m_points;
    int          m_totalPoints = 0;
    QString      m_capNote;
};
