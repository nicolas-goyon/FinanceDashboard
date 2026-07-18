#include "SnapshotImportController.h"
#include "import/ImportTemplate.h"
#include "import/ImportTemplateRegistry.h"
#include "utils/Formatter.h"
#include "utils/AccountHashing.h"
#include "db/Database.h"
#include "models/Account.h"
#include "models/Category.h"
#include "models/ImportLog.h"
#include "models/ImportedStatement.h"
#include "models/Transaction.h"

#include <cmath>
#include <optional>
#include <QDate>
#include <QFileInfo>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <Qt>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

namespace {
constexpr int kStatusClearDelayMs = 4000; // how long a status message stays visible
// Amounts within this many euros/dollars of each other count as "reconciled"
// — guards against float rounding noise, not a real tolerance for genuine
// discrepancies (bank statements are exact to the cent).
constexpr double kReconciliationEpsilon = 0.01;
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
        { "periodFromText",     Fmt::dateOrDash(m_statement.periodFrom) },
        { "periodToText",       Fmt::dateOrDash(m_statement.periodTo) },
        { "balanceStartText",   Fmt::money(m_statement.balanceStart) },
        { "balanceStartDateText", Fmt::dateOrDash(m_statement.balanceStartDate) },
        { "balanceEndText",     Fmt::money(m_statement.balanceEnd) },
        { "balanceEndDateText", Fmt::dateOrDash(m_statement.balanceEndDate) },
        { "transactionCount",   m_statement.transactions.size() },
    };
}

QVariantList SnapshotImportController::transactionRows() const
{
    QVariantList rows;
    rows.reserve(m_statement.transactions.size());
    for (int i = 0; i < m_statement.transactions.size(); ++i) {
        const Data::ImportedTransaction& transaction = m_statement.transactions.at(i);
        rows.append(QVariantMap{
            { "index",             i },
            { "confirmationDate",  Fmt::dateOrDash(transaction.confirmationDate) },
            { "operationDate",     Fmt::dateOrDash(transaction.operationDate) },
            { "name",               transaction.name },
            { "description",       transaction.description },
            { "debitText",         transaction.debit  > 0.0 ? Fmt::money(-transaction.debit) : QStringLiteral("—") },
            { "creditText",        transaction.credit > 0.0 ? Fmt::money(transaction.credit, true) : QStringLiteral("—") },
            { "balanceText",       Fmt::money(transaction.balanceAfter) },
            { "category",          transaction.category },
            { "subcategory",       transaction.subcategory },
            { "confidence",        transaction.confidence },
            { "included",          transaction.included },
        });
    }
    return rows;
}

QVariantList SnapshotImportController::logEntries() const
{
    QVariantList rows;
    rows.reserve(m_log.size());
    for (const Data::ImportLogEntry& entry : m_log) {
        rows.append(QVariantMap{
            { "level",   Data::ImportLogEntry::levelLabel(entry.level) },
            { "line",    entry.lineNumber },
            { "message", entry.message },
            { "rawLine", entry.rawLine },
        });
    }
    return rows;
}

int SnapshotImportController::errorCount() const
{
    int count = 0;
    for (const Data::ImportLogEntry& entry : m_log) {
        if (entry.level == Data::LogLevel::Error) {
            ++count;
        }
    }
    return count;
}

int SnapshotImportController::warningCount() const
{
    int count = 0;
    for (const Data::ImportLogEntry& entry : m_log) {
        if (entry.level == Data::LogLevel::Warning) {
            ++count;
        }
    }
    return count;
}

void SnapshotImportController::parseFile(const QUrl& fileUrl, const QString& templateId)
{
    m_statement = Data::ImportedStatement{};
    m_log.clear();
    m_templateId = templateId; // stashed for save()'s Data::Transaction::sourceTemplate

    const ImportTemplate* tpl = ImportTemplateRegistry::byId(templateId);
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    m_sourceFileName = QFileInfo(path).fileName();

    if (tpl == nullptr) {
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
        const bool success = tpl->parse(path, m_statement, m_log);
        m_hasParsed = success;
        m_lastParseFailed = !success;
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
    if (row < 0 || row >= m_statement.transactions.size()) {
        return;
    }
    // row bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    Data::ImportedTransaction& transaction = m_statement.transactions[row]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (transaction.category == category) {
        return;
    }
    transaction.category = category;
    transaction.subcategory.clear(); // parent changed — old subcategory no longer applies
    emit statementChanged();
}

void SnapshotImportController::setRowSubcategory(int row, const QString& subcategory)
{
    if (row < 0 || row >= m_statement.transactions.size()) {
        return;
    }
    // row bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    m_statement.transactions[row].subcategory = subcategory; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    emit statementChanged();
}

void SnapshotImportController::setRowName(int row, const QString& name)
{
    if (row < 0 || row >= m_statement.transactions.size()) {
        return;
    }
    // row bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    Data::ImportedTransaction& transaction = m_statement.transactions[row]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (transaction.name == name) {
        return;
    }
    transaction.name = name;
    emit statementChanged();
}

void SnapshotImportController::setRowIncluded(int row, bool included)
{
    if (row < 0 || row >= m_statement.transactions.size()) {
        return;
    }
    // row bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    Data::ImportedTransaction& transaction = m_statement.transactions[row]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (transaction.included == included) {
        return; // no-op guard — also avoids the Repeater "model" binding-loop
                // warning this used to cause: transactions default to
                // included=true while CheckBox.checked defaults to false, so
                // the delegate's initial binding assignment used to fire
                // onCheckedChanged -> setRowIncluded(true) -> statementChanged()
                // re-entrantly while the model itself was still being built.
    }
    transaction.included = included;
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
    if (targetHash.isEmpty()) {
        return -1;
    }

    for (int i = 0; i < accounts.size(); ++i) {
        const QVariantMap acc = accounts.at(i).toMap();
        const QVariantList hashes = acc.value("hashes").toList();
        for (const QVariant& hashEntry : hashes) {
            if (hashEntry.toMap().value("hash").toString().compare(targetHash, Qt::CaseInsensitive) == 0) {
                return i;
            }
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

    // Snapshot the reconciliation result *before* writing anything — the
    // report should show what was actually checked at save time, not a
    // recomputation against a statement the user may have kept editing.
    const QVariantMap reconciliation = checkReconciliation(accountId);

    QVector<Data::Transaction> toSave;
    toSave.reserve(m_statement.transactions.size());
    for (const Data::ImportedTransaction& transaction : m_statement.transactions) {
        if (!transaction.included) {
            continue;
        }
        Data::Transaction row;
        row.accountId        = accountId;
        row.confirmationDate = transaction.confirmationDate;
        row.operationDate    = transaction.operationDate;
        row.name              = transaction.name;
        row.description      = transaction.description;
        row.debit            = transaction.debit;
        row.credit           = transaction.credit;
        row.balanceAfter     = transaction.balanceAfter;
        row.category         = transaction.category;
        row.subcategory      = transaction.subcategory;
        // transaction.confidence is deliberately dropped here, not copied —
        // it's a review-screen-only hint (see Data::ImportedTransaction::
        // confidence), never persisted alongside the category/subcategory
        // it helped the user land on.
        row.sourceTemplate   = m_templateId;
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
    if (!hash.isEmpty() && !Database::instance().accountHasHash(accountId, hash)) {
        Database::instance().addAccountHash(accountId, hash, QStringLiteral("auto"));
    }

    // "Remember the start and end amount from the CSV" (product request) —
    // the statement's own declared balanceStart/balanceEnd become balance
    // checkpoints, same mechanism as manually-added ones (see § Balance
    // Checkpoints), so both this import's own reconciliation-check-1 next
    // time and the Account History line chart have a real anchor even
    // before the user ever opens Manage Accounts to add one by hand.
    rememberStatementBalance(accountId, m_statement.balanceStartDate, m_statement.balanceStart,
                              QStringLiteral("Auto — statement start (%1)").arg(m_sourceFileName));
    rememberStatementBalance(accountId, m_statement.balanceEndDate, m_statement.balanceEnd,
                              QStringLiteral("Auto — statement end (%1)").arg(m_sourceFileName));

    buildImportReport(accountLabel, toSave, reconciliation);
    emit reportChanged();

    setStatus(QString("✓  Saved %1 of %2 transaction(s) to \"%3\"")
                   .arg(toSave.size())
                   .arg(m_statement.transactions.size())
                   .arg(accountLabel));
}

QVariantMap SnapshotImportController::checkReconciliation(int accountId) const
{
    QVariantMap result;
    result["available"] = m_hasParsed && !m_lastParseFailed && accountId >= 0;
    if (!result["available"].toBool()) {
        return result;
    }

    // ── Check 1: declared start vs. our own last record before that date ──
    const std::optional<Data::AccountBalanceCheckpoint> checkpoint =
        Database::instance().latestCheckpointOnOrBefore(accountId, m_statement.balanceStartDate);
    const std::optional<Data::Transaction> priorTx =
        Database::instance().latestTransactionOnOrBefore(accountId, m_statement.balanceStartDate);

    bool hasStartReference = false;
    double expectedStart = 0.0;
    QString startSourceLabel;
    QDate startSourceDate;
    // Prefer whichever reference is dated closer to (i.e. more recent
    // before) the statement's own start date — a checkpoint and a
    // transaction can both exist; the most recent one is the better
    // "what did we last know" answer.
    if (checkpoint.has_value() && (!priorTx.has_value() || checkpoint->date >= priorTx->operationDate)) {
        hasStartReference = true;
        expectedStart = checkpoint->balance;
        startSourceLabel = QStringLiteral("balance checkpoint");
        startSourceDate = checkpoint->date;
    } else if (priorTx.has_value()) {
        hasStartReference = true;
        expectedStart = priorTx->balanceAfter;
        startSourceLabel = QStringLiteral("last recorded transaction");
        startSourceDate = priorTx->operationDate;
    }

    result["hasStartReference"]   = hasStartReference;
    result["startSourceLabel"]    = startSourceLabel;
    result["startSourceDateText"] = Fmt::dateOrDash(startSourceDate);
    result["startDeclaredText"]   = Fmt::money(m_statement.balanceStart);
    result["startExpectedText"]   = hasStartReference ? Fmt::money(expectedStart) : QStringLiteral("—");
    const double startDiff = m_statement.balanceStart - expectedStart;
    result["startOk"]       = !hasStartReference || std::abs(startDiff) < kReconciliationEpsilon;
    result["startDiffText"] = hasStartReference ? Fmt::money(startDiff, true) : QStringLiteral("—");

    // ── Check 2: declared start + kept (included) rows vs. declared end ────
    double sumIncluded = 0.0;
    for (const Data::ImportedTransaction& transaction : m_statement.transactions) {
        if (transaction.included) {
            sumIncluded += (transaction.credit - transaction.debit);
        }
    }
    const double computedEnd = m_statement.balanceStart + sumIncluded;
    const double endDiff = m_statement.balanceEnd - computedEnd;

    result["endComputedText"] = Fmt::money(computedEnd);
    result["endDeclaredText"] = Fmt::money(m_statement.balanceEnd);
    result["endDiffText"]     = Fmt::money(endDiff, true);
    result["endDiffRaw"]      = endDiff;
    result["endOk"]           = std::abs(endDiff) < kReconciliationEpsilon;

    return result;
}

void SnapshotImportController::addReconciliationAdjustment(int accountId)
{
    const QVariantMap check = checkReconciliation(accountId);
    if (!check.value(QStringLiteral("available")).toBool() || check.value(QStringLiteral("endOk")).toBool()) {
        return; // nothing parsed, or already reconciled — nothing to add
    }

    const double diff = check.value(QStringLiteral("endDiffRaw")).toDouble(); // declared end - computed end
    static const QString kAdjustmentCategory = QStringLiteral("Reconciliation Adjustment");
    ensureCategoryExists(kAdjustmentCategory);

    Data::ImportedTransaction adjustment;
    adjustment.operationDate = m_statement.balanceEndDate.isValid() ? m_statement.balanceEndDate : m_statement.periodTo;
    adjustment.confirmationDate = adjustment.operationDate;
    adjustment.name = QStringLiteral("Reconciliation adjustment");
    adjustment.description = QStringLiteral(
        "Auto-added so the kept transactions match the statement's declared closing balance.");
    // diff > 0: declared end is HIGHER than what the kept rows add up to —
    // we're missing a credit. diff < 0: declared end is LOWER — missing a
    // debit.
    if (diff > 0.0) {
        adjustment.credit = diff;
        adjustment.debit  = 0.0;
    } else {
        adjustment.debit  = -diff;
        adjustment.credit = 0.0;
    }
    adjustment.balanceAfter = m_statement.balanceEnd;
    adjustment.category     = kAdjustmentCategory;
    adjustment.included     = true;

    m_statement.transactions.append(adjustment);
    emit statementChanged();
    setStatus(QString("✓  Added a %1 adjustment transaction to match the declared closing balance")
                   .arg(Fmt::money(diff, true)));
}

void SnapshotImportController::ensureCategoryExists(const QString& name)
{
    const QVector<Data::Category> categories = Database::instance().loadCategories();
    for (const Data::Category& category : categories) {
        if (category.name.compare(name, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    // Neutral grey — matches the "Other" default category's color, since
    // this row isn't a spending/income category the user chose on purpose.
    Database::instance().addCategory(name, QStringLiteral("#6c7086"));
}

void SnapshotImportController::rememberStatementBalance(int accountId, const QDate& date, double balance, const QString& note)
{
    if (!date.isValid()) {
        return;
    }
    const QVector<Data::AccountBalanceCheckpoint> existing = Database::instance().loadBalanceCheckpoints(accountId);
    for (const Data::AccountBalanceCheckpoint& checkpoint : existing) {
        if (checkpoint.date == date) {
            return; // already have a checkpoint for this exact date — never overwritten automatically
        }
    }
    Database::instance().addBalanceCheckpoint(accountId, date, balance, note);
}

void SnapshotImportController::buildImportReport(const QString& accountLabel, const QVector<Data::Transaction>& saved,
                                                   const QVariantMap& reconciliation)
{
    double totalIncome = 0.0;
    double totalExpense = 0.0;
    // Category -> net total (credit - debit); Category -> Subcategory -> net total.
    QMap<QString, double> categoryTotals;
    QMap<QString, QMap<QString, double>> subcategoryTotals;

    for (const Data::Transaction& transaction : saved) {
        totalIncome  += transaction.credit;
        totalExpense += transaction.debit;

        const QString category    = transaction.category.isEmpty() ? QStringLiteral("Uncategorized") : transaction.category;
        const QString subcategory = transaction.subcategory.isEmpty() ? QStringLiteral("—") : transaction.subcategory;
        const double net = transaction.credit - transaction.debit;

        categoryTotals[category] += net;
        subcategoryTotals[category][subcategory] += net;
    }

    QVariantList byCategory;
    for (auto categoryIt = categoryTotals.constBegin(); categoryIt != categoryTotals.constEnd(); ++categoryIt) {
        QVariantList subcategories;
        const QMap<QString, double> subMap = subcategoryTotals.value(categoryIt.key());
        for (auto subIt = subMap.constBegin(); subIt != subMap.constEnd(); ++subIt) {
            subcategories.append(QVariantMap{
                { "subcategory", subIt.key() },
                { "totalText",   Fmt::money(subIt.value(), true) },
                { "totalRaw",    subIt.value() },
            });
        }
        byCategory.append(QVariantMap{
            { "category",      categoryIt.key() },
            { "totalText",     Fmt::money(categoryIt.value(), true) },
            { "totalRaw",      categoryIt.value() },
            { "subcategories", subcategories },
        });
    }

    m_lastImportReport = QVariantMap{
        { "accountLabel",      accountLabel },
        { "transactionCount",  saved.size() },
        { "totalIncomeText",   Fmt::money(totalIncome, true) },
        { "totalExpenseText",  Fmt::money(-totalExpense, true) },
        { "netChangeText",     Fmt::money(totalIncome - totalExpense, true) },
        { "byCategory",        byCategory },
        { "reconciliation",    reconciliation },
    };
}

void SnapshotImportController::setStatus(const QString& msg)
{
    m_statusText = msg;
    emit statusChanged();
    QTimer::singleShot(kStatusClearDelayMs, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}
