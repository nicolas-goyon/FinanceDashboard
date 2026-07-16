#include "SnapshotImportController.h"
#include "import/ImportTemplate.h"
#include "import/ImportTemplateRegistry.h"
#include "utils/Formatter.h"
#include "utils/AccountHashing.h"
#include "db/Database.h"
#include "models/Transaction.h"

#include <QFileInfo>
#include <QTimer>

namespace {

QString dateText(const QDate& d)
{
    return d.isValid() ? d.toString(QStringLiteral("dd/MM/yyyy")) : QStringLiteral("—");
}

} // namespace

SnapshotImportController::SnapshotImportController(QObject* parent)
    : QObject(parent)
{
    for (const ImportTemplate* tpl : ImportTemplateRegistry::all()) {
        m_templates.append(QVariantMap{
            { "id",          tpl->id() },
            { "displayName", tpl->displayName() },
            { "fileFilter",  tpl->fileFilter() },
        });
    }
}

QVariantMap SnapshotImportController::statementInfo() const
{
    const QString hash = AccountHashing::compute(m_statement.iban, m_statement.bic);

    return QVariantMap{
        { "holderName",         m_statement.accountHolderName.isEmpty() ? QStringLiteral("—") : m_statement.accountHolderName },
        { "accountName",        m_statement.accountName.isEmpty()       ? QStringLiteral("—") : m_statement.accountName },
        { "clientCode",         m_statement.clientCode.isEmpty()        ? QStringLiteral("—") : m_statement.clientCode },
        { "accountNumber",      m_statement.accountNumber.isEmpty()     ? QStringLiteral("—") : m_statement.accountNumber },
        { "iban",               m_statement.iban.isEmpty()              ? QStringLiteral("—") : m_statement.iban },
        { "bic",                m_statement.bic.isEmpty()               ? QStringLiteral("—") : m_statement.bic },
        // HMAC-SHA256(iban, key=bic) — see AccountHashing::compute(). Empty IBAN → "—".
        { "accountHash",        hash.isEmpty() ? QStringLiteral("—") : hash },
        // Same value, un-prettified (empty string instead of "—") — what
        // ImportSnapshotPage.qml's "Link this account" button actually
        // passes to AccountController.linkAccountHash().
        { "accountHashRaw",     hash },
        { "periodFromText",     dateText(m_statement.periodFrom) },
        { "periodToText",       dateText(m_statement.periodTo) },
        { "balanceStartText",   Fmt::money(m_statement.balanceStart) },
        { "balanceStartDateText", dateText(m_statement.balanceStartDate) },
        { "balanceEndText",     Fmt::money(m_statement.balanceEnd) },
        { "balanceEndDateText", dateText(m_statement.balanceEndDate) },
        { "transactionCount",   m_statement.transactions.size() },
    };
}

QVariantList SnapshotImportController::transactionRows() const
{
    QVariantList rows;
    rows.reserve(m_statement.transactions.size());
    for (int i = 0; i < m_statement.transactions.size(); ++i) {
        const Data::ImportedTransaction& tx = m_statement.transactions.at(i);
        rows.append(QVariantMap{
            { "index",             i },
            { "confirmationDate",  dateText(tx.confirmationDate) },
            { "operationDate",     dateText(tx.operationDate) },
            { "description",       tx.description },
            { "debitText",         tx.debit  > 0.0 ? Fmt::money(-tx.debit) : QStringLiteral("—") },
            { "creditText",        tx.credit > 0.0 ? Fmt::money(tx.credit, true) : QStringLiteral("—") },
            { "balanceText",       Fmt::money(tx.balanceAfter) },
            { "category",          tx.category },
            { "subcategory",       tx.subcategory },
            { "included",          tx.included },
        });
    }
    return rows;
}

QVariantList SnapshotImportController::logEntries() const
{
    QVariantList rows;
    rows.reserve(m_log.size());
    for (const Data::ImportLogEntry& e : m_log) {
        rows.append(QVariantMap{
            { "level",   Data::ImportLogEntry::levelLabel(e.level) },
            { "line",    e.lineNumber },
            { "message", e.message },
            { "rawLine", e.rawLine },
        });
    }
    return rows;
}

int SnapshotImportController::errorCount() const
{
    int n = 0;
    for (const Data::ImportLogEntry& e : m_log)
        if (e.level == Data::LogLevel::Error)
            ++n;
    return n;
}

int SnapshotImportController::warningCount() const
{
    int n = 0;
    for (const Data::ImportLogEntry& e : m_log)
        if (e.level == Data::LogLevel::Warning)
            ++n;
    return n;
}

void SnapshotImportController::parseFile(const QUrl& fileUrl, const QString& templateId)
{
    m_statement = Data::ImportedStatement{};
    m_log.clear();
    m_templateId = templateId; // stashed for save()'s Data::Transaction::sourceTemplate

    const ImportTemplate* tpl = ImportTemplateRegistry::byId(templateId);
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    m_sourceFileName = QFileInfo(path).fileName();

    if (!tpl) {
        m_log.append(Data::ImportLogEntry{ Data::LogLevel::Error, 0,
            QStringLiteral("Unknown template id: %1").arg(templateId), QString() });
        m_hasParsed = false;
        m_lastParseFailed = true;
    } else if (path.isEmpty()) {
        m_log.append(Data::ImportLogEntry{ Data::LogLevel::Error, 0,
            QStringLiteral("No file selected"), QString() });
        m_hasParsed = false;
        m_lastParseFailed = true;
    } else {
        const bool ok = tpl->parse(path, m_statement, m_log);
        m_hasParsed = ok;
        m_lastParseFailed = !ok;
    }

    emit statementChanged();
    emit logChanged();

    if (m_lastParseFailed) {
        setStatus(QStringLiteral("⚠  Import failed — see the log for details"));
    } else {
        setStatus(QString("✓  Parsed %1 transaction(s) from %2")
                       .arg(m_statement.transactions.size())
                       .arg(m_sourceFileName));
    }
}

void SnapshotImportController::setRowCategory(int row, const QString& category)
{
    if (row < 0 || row >= m_statement.transactions.size())
        return;
    Data::ImportedTransaction& tx = m_statement.transactions[row];
    if (tx.category == category)
        return;
    tx.category = category;
    tx.subcategory.clear(); // parent changed — old subcategory no longer applies
    emit statementChanged();
}

void SnapshotImportController::setRowSubcategory(int row, const QString& subcategory)
{
    if (row < 0 || row >= m_statement.transactions.size())
        return;
    m_statement.transactions[row].subcategory = subcategory;
    emit statementChanged();
}

void SnapshotImportController::setRowIncluded(int row, bool included)
{
    if (row < 0 || row >= m_statement.transactions.size())
        return;
    Data::ImportedTransaction& tx = m_statement.transactions[row];
    if (tx.included == included)
        return; // no-op guard — also avoids the Repeater "model" binding-loop
                // warning this used to cause: transactions default to
                // included=true while CheckBox.checked defaults to false, so
                // the delegate's initial binding assignment used to fire
                // onCheckedChanged -> setRowIncluded(true) -> statementChanged()
                // re-entrantly while the model itself was still being built.
    tx.included = included;
    emit statementChanged();
}

int SnapshotImportController::matchAccountIndex(const QVariantList& accounts) const
{
    // Sole match path: compare the statement's fingerprint against each
    // candidate account's *stored* hashes list (account_hashes table, via
    // AccountController — see § Account Management). Never raw IBAN/BIC/
    // account-number comparisons — accounts don't even store those anymore
    // (removed for security, see § Account Management), so an account only
    // ever matches if it has actually been linked (see
    // AccountController::linkAccountHash()/addHash()). No hash on the
    // statement, or no account with a matching stored hash → -1, caller
    // shows the manual picker.
    const QString targetHash = AccountHashing::compute(m_statement.iban, m_statement.bic);
    if (targetHash.isEmpty())
        return -1;

    for (int i = 0; i < accounts.size(); ++i) {
        const QVariantMap acc = accounts.at(i).toMap();
        const QVariantList hashes = acc.value("hashes").toList();
        for (const QVariant& hv : hashes) {
            if (hv.toMap().value("hash").toString().compare(targetHash, Qt::CaseInsensitive) == 0)
                return i;
        }
    }

    return -1;
}

void SnapshotImportController::reset()
{
    m_statement = Data::ImportedStatement{};
    m_log.clear();
    m_hasParsed = false;
    m_lastParseFailed = false;
    m_sourceFileName.clear();
    m_templateId.clear();
    emit statementChanged();
    emit logChanged();
}

void SnapshotImportController::save(int accountId, const QString& accountLabel)
{
    if (accountId < 0) {
        setStatus(QStringLiteral("⚠  Please select an account before saving"));
        return;
    }

    QVector<Data::Transaction> toSave;
    toSave.reserve(m_statement.transactions.size());
    for (const Data::ImportedTransaction& tx : m_statement.transactions) {
        if (!tx.included)
            continue;
        Data::Transaction row;
        row.accountId         = accountId;
        row.confirmationDate  = tx.confirmationDate;
        row.operationDate     = tx.operationDate;
        row.description        = tx.description;
        row.debit              = tx.debit;
        row.credit              = tx.credit;
        row.balanceAfter        = tx.balanceAfter;
        row.category            = tx.category;
        row.subcategory          = tx.subcategory;
        row.sourceTemplate       = m_templateId;
        toSave.append(row);
    }

    if (!Database::instance().insertTransactions(accountId, toSave)) {
        setStatus(QStringLiteral("⚠  Failed to save transactions — see application log"));
        return;
    }

    // Auto-link: if this statement has a computable fingerprint and the
    // chosen account doesn't have it on file yet, store it now (source
    // "auto") — future imports of the same account then auto-match (see §
    // Statement Import / § Account Management in ARCHITECTURE.MD).
    const QString hash = AccountHashing::compute(m_statement.iban, m_statement.bic);
    if (!hash.isEmpty() && !Database::instance().accountHasHash(accountId, hash))
        Database::instance().addAccountHash(accountId, hash, QStringLiteral("auto"));

    setStatus(QString("✓  Saved %1 of %2 transaction(s) to \"%3\"")
                   .arg(toSave.size())
                   .arg(m_statement.transactions.size())
                   .arg(accountLabel));
}

void SnapshotImportController::setStatus(const QString& msg)
{
    m_statusText = msg;
    emit statusChanged();
    QTimer::singleShot(4000, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}
