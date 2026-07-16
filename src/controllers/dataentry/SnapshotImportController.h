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
// Persistence is REAL as of § Account Management (ARCHITECTURE.MD):
// save() writes the included rows to the `transactions` table
// (Database::insertTransactions()) and auto-links the statement's account
// hash to the chosen account if it isn't already stored there
// (Database::accountHasHash()/addAccountHash()) — see save()'s doc comment
// below for the exact contract with the QML side (accounts now come from
// AccountController, not this class).

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
    // AccountHashing::compute(), src/utils/AccountHashing.h), the same
    // fingerprint matchAccountIndex() uses to find the target account, shown
    // here so it's visible/auditable ("accountHashRaw" is the same value
    // un-prettified, for the "Link this account" action).
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

    // Given AccountController.accounts ([{id, name, type, category,
    // hashes: [{hash, ...}], ...}, ...]), returns the index of the account
    // matching this statement, or -1 if none matched (caller should ask the
    // user). Doesn't compare IBAN/BIC fields directly — accounts don't even
    // store those (removed for security, see § Account Management): the
    // statement's IBAN/BIC is folded into a fingerprint (AccountHashing::
    // compute(), § Statement Import in ARCHITECTURE.MD) and compared against
    // each candidate account's *stored* hashes list (account_hashes table,
    // via AccountController). No account-number fallback anymore — hash is
    // the only match path, so an account with none stored just shows as
    // unmatched until linked (see the "not linked" badge / "Link this
    // account" action on ImportSnapshotPage.qml).
    Q_INVOKABLE int matchAccountIndex(const QVariantList& accounts) const;

    // Clears the parsed statement and returns to the file/template picker.
    Q_INVOKABLE void reset();

    // Persists every included transaction row to the `transactions` table
    // for `accountId` (a real AccountController account id — QML passes
    // AccountController.accounts[selectedIndex].id) via
    // Database::insertTransactions(). Also auto-links this statement's
    // account hash to that account (Database::addAccountHash(), source
    // "auto") if it isn't stored there yet, so future imports of the same
    // account auto-match. accountId < 0 (nothing selected) fails without
    // writing anything. accountLabel is only used for the status message.
    // The QML side must call AccountController.reload() afterward so the
    // singleton's cached accounts/hashes pick up this write (see
    // AccountController::reload() doc comment).
    Q_INVOKABLE void save(int accountId, const QString& accountLabel);

signals:
    void statementChanged();
    void logChanged();
    void statusChanged();

private:
    void setStatus(const QString& msg);

    QVariantList m_templates;

    Data::ImportedStatement       m_statement;
    QVector<Data::ImportLogEntry> m_log;
    bool    m_hasParsed{false};
    bool    m_lastParseFailed{false};
    QString m_sourceFileName;
    QString m_templateId; // last parseFile()'s templateId — stored on saved transactions as source_template

    QString m_statusText;
};
