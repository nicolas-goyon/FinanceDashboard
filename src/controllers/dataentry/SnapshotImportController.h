#ifndef FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_SNAPSHOTIMPORTCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_SNAPSHOTIMPORTCONTROLLER_H

#include "models/ImportedStatement.h"
#include "models/ImportLog.h"
#include "models/Transaction.h"

#include <QDate>
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
// data — the CSV parsing in SumeriaCcpCsvExtractionTemplate is real), review/edit the
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
    // [{ index, confirmationDate, operationDate, name, description, debitText,
    //    creditText, balanceText, category, subcategory, confidence, included }]
    Q_PROPERTY(QVariantList transactionRows READ transactionRows NOTIFY statementChanged)

    // [{ level, line, message, rawLine }]
    Q_PROPERTY(QVariantList logEntries READ logEntries NOTIFY logChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY logChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY logChanged)

    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

    // Filled in by save() right after a successful write — see
    // lastImportReport()'s doc comment and buildImportReport() for the exact
    // shape. Backs the "step 2" report view on ImportSnapshotPage.qml (see
    // ARCHITECTURE.MD § Statement Import — Import Report).
    Q_PROPERTY(QVariantMap lastImportReport READ lastImportReport NOTIFY reportChanged)

public:
    explicit SnapshotImportController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList templates() const { return m_templates; }

    [[nodiscard]] bool    hasParsed() const       { return m_hasParsed; }
    [[nodiscard]] bool    lastParseFailed() const { return m_lastParseFailed; }
    [[nodiscard]] QString sourceFileName() const  { return m_sourceFileName; }

    [[nodiscard]] QVariantMap  statementInfo() const;
    [[nodiscard]] QVariantList transactionRows() const;

    [[nodiscard]] QVariantList logEntries() const;
    [[nodiscard]] int errorCount() const;
    [[nodiscard]] int warningCount() const;

    [[nodiscard]] QString statusText() const { return m_statusText; }

    [[nodiscard]] QVariantMap lastImportReport() const { return m_lastImportReport; }

    // Run the selected template's parser over the picked file.
    Q_INVOKABLE void parseFile(const QUrl& fileUrl, const QString& templateId);

    // Row edits (index into the current statement's transactions).
    Q_INVOKABLE void setRowCategory(int row, const QString& category);
    Q_INVOKABLE void setRowSubcategory(int row, const QString& subcategory);
    Q_INVOKABLE void setRowIncluded(int row, bool included);
    // Lets the user correct the auto-extracted name (see
    // extractTransactionName() in SumeriaCcpCsvExtractionTemplate.cpp) before
    // saving — the full description is unaffected and always stays as parsed.
    Q_INVOKABLE void setRowName(int row, const QString& name);

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
    Q_INVOKABLE [[nodiscard]] int matchAccountIndex(const QVariantList& accounts) const;

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

    // ── Reconciliation (see ARCHITECTURE.MD § Statement Import — Import
    //    Report / § Balance Checkpoints) ─────────────────────────────────────
    // Two checks against the *currently parsed* statement (m_statement),
    // both read-only — safe to call repeatedly (e.g. every time the "keep"
    // checkboxes change) before the user actually hits Save:
    //   Check 1 ("start balance") — the statement's own declared
    //     balanceStart vs. the last balance we have on record for this
    //     account on or before balanceStartDate (the latest
    //     AccountBalanceCheckpoint, or failing that the latest saved
    //     transaction's balanceAfter). Informational only — if there's no
    //     prior record at all (e.g. the very first import for this account,
    //     nothing checkpointed either), the check is skipped entirely
    //     ("hasStartReference": false) rather than compared against
    //     something unrelated like today's balance.
    //   Check 2 ("end balance") — declared balanceStart + sum(credit-debit)
    //     over every currently-*included* row vs. the statement's own
    //     declared balanceEnd. This is the one the Save flow blocks on:
    //     excluding ("un-keeping") a row the bank itself counted breaks this
    //     equation. See addReconciliationAdjustment() for the fix path.
    // Returns "available": false (nothing else meaningful) if nothing has
    // been parsed yet or accountId is invalid.
    Q_INVOKABLE [[nodiscard]] QVariantMap checkReconciliation(int accountId) const;

    // Appends one synthetic ImportedTransaction to the current statement to
    // close check 2's gap exactly — the "add a factice value... with a
    // special category attached" remedy from the product request. Dated at
    // the statement's balanceEndDate (falls back to periodTo), category
    // "Reconciliation Adjustment" (created in the categories table first if
    // it doesn't exist yet — QML must call CategoryController.reload()
    // afterward so the singleton picks it up, same "no C++-to-C++ singleton
    // coupling" pattern as save()'s account-hash auto-link). No-op if
    // checkReconciliation() reports the check already passes (or nothing is
    // parsed / accountId is invalid).
    Q_INVOKABLE void addReconciliationAdjustment(int accountId);

signals:
    void statementChanged();
    void logChanged();
    void statusChanged();
    // Emitted after save() successfully builds a new lastImportReport().
    void reportChanged();

private:
    void setStatus(const QString& msg);
    // Adds `name` as a new category (neutral color) if no existing category
    // matches it case-insensitively — used by addReconciliationAdjustment()
    // to make sure "Reconciliation Adjustment" exists before a row is
    // tagged with it. Writes straight to Database, same "no C++-to-C++
    // singleton coupling with CategoryController" reasoning as everywhere
    // else in this class.
    static void ensureCategoryExists(const QString& name);
    // Auto-remembers a statement-declared balance ("remember the start and
    // end amount from the CSV" — product request) as an
    // AccountBalanceCheckpoint, unless the account already has one dated
    // exactly `date` (never overwrites an existing checkpoint — including
    // one this same helper added on a prior import). No-op if `date` is
    // invalid. See ARCHITECTURE.MD § Statement Import — Import Report.
    static void rememberStatementBalance(int accountId, const QDate& date, double balance, const QString& note);
    // Builds m_lastImportReport from the rows actually written by save()
    // (not from m_statement, which the user could still be editing by the
    // time the report is viewed) plus the reconciliation snapshot taken
    // right before the write.
    void buildImportReport(const QString& accountLabel, const QVector<Data::Transaction>& saved,
                            const QVariantMap& reconciliation);

    QVariantList m_templates;

    Data::ImportedStatement       m_statement;
    QVector<Data::ImportLogEntry> m_log;
    bool    m_hasParsed{false};
    bool    m_lastParseFailed{false};
    QString m_sourceFileName;
    QString m_templateId; // last parseFile()'s templateId — stored on saved transactions as source_template

    QString m_statusText;
    QVariantMap m_lastImportReport;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_SNAPSHOTIMPORTCONTROLLER_H
