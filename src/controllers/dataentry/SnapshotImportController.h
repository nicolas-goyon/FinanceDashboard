#pragma once

#include "models/ImportedStatement.h"
#include "models/ImportLog.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqml.h>

// ── SnapshotImportController ──────────────────────────────────────────────────
//
// Backend for qml/pages/ImportSnapshotPage.qml, reached from the sidebar's
// Data Entry → "New Monthly Snapshot" action. Wraps the ImportTemplate
// system (src/import/): pick a template + file, parse it for real (no mock
// data — the CSV parsing in CheckingCsvTemplateV1 is real), review/edit the
// resulting transactions and categories, inspect parse warnings/errors via
// the log, then Save.
//
// Scope for this pass (matches the rest of Data Entry): parsing is REAL,
// persistence is MOCK — save() only sets statusText, same convention as
// DataEntryController::saveSnapshot(). DB schema for transactions is a
// PROJECT.md / ARCHITECTURE.MD TODO (see "Optional categorized transactions").

class SnapshotImportController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ id, displayName, fileFilter }] — available ImportTemplateRegistry entries
    Q_PROPERTY(QVariantList templates READ templates CONSTANT)

    // true once parseFile() has run and produced a usable statement (even if
    // some rows were skipped — check logEntries/errorCount for issues).
    Q_PROPERTY(bool hasParsed READ hasParsed NOTIFY statementChanged)
    // true if the last parseFile() call failed outright (see logEntries for why).
    Q_PROPERTY(bool lastParseFailed READ lastParseFailed NOTIFY statementChanged)
    Q_PROPERTY(QString sourceFileName READ sourceFileName NOTIFY statementChanged)

    // Formatted header fields — see statementInfo() for the shape. Includes
    // "accountHash" (HMAC-SHA256 of the IBAN keyed by the BIC — see
    // accountHash() in the .cpp), the same fingerprint matchAccountIndex()
    // uses to find the target account, shown here so it's visible/auditable.
    Q_PROPERTY(QVariantMap statementInfo READ statementInfo NOTIFY statementChanged)
    // [{ index, confirmationDate, operationDate, description, debitText,
    //    creditText, balanceText, category, subcategory, included }]
    Q_PROPERTY(QVariantList transactionRows READ transactionRows NOTIFY statementChanged)

    // [{ level, line, message, rawLine }]
    Q_PROPERTY(QVariantList logEntries READ logEntries NOTIFY logChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY logChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY logChanged)

    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit SnapshotImportController(QObject* parent = nullptr);

    QVariantList templates() const { return m_templates; }

    bool    hasParsed() const       { return m_hasParsed; }
    bool    lastParseFailed() const { return m_lastParseFailed; }
    QString sourceFileName() const  { return m_sourceFileName; }

    QVariantMap  statementInfo() const;
    QVariantList transactionRows() const;

    QVariantList logEntries() const;
    int errorCount() const;
    int warningCount() const;

    QString statusText() const { return m_statusText; }

    // Run the selected template's parser over the picked file.
    Q_INVOKABLE void parseFile(const QUrl& fileUrl, const QString& templateId);

    // Row edits (index into the current statement's transactions).
    Q_INVOKABLE void setRowCategory(int row, const QString& category);
    Q_INVOKABLE void setRowSubcategory(int row, const QString& subcategory);
    Q_INVOKABLE void setRowIncluded(int row, bool included);

    // Given the Data Entry accounts list ([{name, type, iban, bic, number}, ...]),
    // returns the index of the account matching this statement, or -1 if none
    // matched (caller should ask the user). Doesn't compare IBAN/BIC fields
    // directly: both sides are folded into an accountHash() fingerprint first
    // and only the fingerprints are compared (mocks a lookup-by-fingerprint
    // DB call — see accountHash() in the .cpp and § Statement Import in
    // ARCHITECTURE.MD). Falls back to a direct account-number match when no
    // hash is available on one side (e.g. an account with no IBAN on file).
    Q_INVOKABLE int matchAccountIndex(const QVariantList& accounts) const;

    // Clears the parsed statement and returns to the file/template picker.
    Q_INVOKABLE void reset();

    // Mock save — see class comment. accountLabel is only used for the status message.
    Q_INVOKABLE void save(const QString& accountLabel);

signals:
    void statementChanged();
    void logChanged();
    void statusChanged();

private:
    void setStatus(const QString& msg);

    // See the .cpp for the rationale — HMAC-SHA256 of the IBAN keyed by the
    // BIC, used instead of direct IBAN/BIC comparisons for account matching.
    static QString accountHash(const QString& iban, const QString& bic);

    QVariantList m_templates;

    Data::ImportedStatement       m_statement;
    QVector<Data::ImportLogEntry> m_log;
    bool    m_hasParsed{false};
    bool    m_lastParseFailed{false};
    QString m_sourceFileName;

    QString m_statusText;
};
