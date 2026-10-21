#include "SnapshotImportController.h"
#include "import/ImportTemplate.h"
#include "import/ImportTemplateRegistry.h"
#include "utils/Formatter.h"
#include "utils/AccountHashing.h"
#include "utils/Money.h"
#include "db/Database.h"
#include "db/tables/TransactionsTable.h"
#include "models/Account.h"
#include "models/Category.h"
#include "models/CategoryRule.h"
#include "models/ImportLog.h"
#include "models/ImportedStatement.h"
#include "models/Transaction.h"
#include "predict/CategoryPredictor.h"
#include "predict/LabelNormalizer.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <QDate>
#include <QFileInfo>
#include <QHash>
#include <QLatin1String>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <Qt>
#include <QtTypes>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

namespace {
constexpr int kStatusClearDelayMs = 4000; // how long a status message stays visible
// Percent scale for the predictor's evaluation numbers (evaluatePredictor()).
constexpr double kPercentScale = 100.0;

// A row that has the bank reference and the amount of a saved transaction,
// but another operation date. The identity by reference assumes one
// reference per transaction; such a row says the bank may reuse them. It is
// skipped like any saved row, and the import report links to the issue
// (TransactionsTable::kReusedReferenceIssueUrl, maintainer decision of
// 2026-10-18).
bool hasReusedReference(const Data::Transaction& row, const QDate& savedOperationDate)
{
    return !TransactionsTable::bankReference(row.description).isEmpty() && savedOperationDate != row.operationDate;
}

// credit - debit, the signed amount the category predictor wants (see
// Predict::CategoryPredictor::predict()). Both fields are magnitudes with the
// other one zero, so this is exact rather than a convention.
double signedAmountOf(const Data::ImportedTransaction& transaction)
{
    return transaction.credit - transaction.debit;
}

// The date a transaction actually happened, falling back to the bank's
// booking date — same preference as Predict::CategoryPredictor::exampleFrom(),
// and it must stay the same or recurrence detection compares mismatched dates.
QDate effectiveDateOf(const Data::ImportedTransaction& transaction)
{
    return transaction.operationDate.isValid() ? transaction.operationDate
                                                : transaction.confirmationDate;
}
} // namespace

SnapshotImportController::SnapshotImportController(QObject* parent)
    : QObject(parent)
    , m_status(this, kStatusClearDelayMs, [this]() { emit statusChanged(); })
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
        // The fingerprint from before #50 (an 8-character BIC), or empty:
        // "Link this account" replaces it too.
        { "legacyHashRaw",      AccountHashing::legacyCompute(m_statement.iban, m_statement.bic) },
        { "periodFromText",     Fmt::dateOrDash(m_statement.periodFrom) },
        { "periodToText",       Fmt::dateOrDash(m_statement.periodTo) },
        // A balance line that could not be read has no date (#44): "—", not 0.
        { "balanceStartText",   m_statement.balanceStartDate.isValid() ? Fmt::money(m_statement.balanceStart)
                                                                       : QStringLiteral("—") },
        { "balanceStartDateText", Fmt::dateOrDash(m_statement.balanceStartDate) },
        { "balanceEndText",     m_statement.balanceEndDate.isValid() ? Fmt::money(m_statement.balanceEnd)
                                                                     : QStringLiteral("—") },
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
            // Pre-formatted here rather than bucketed in QML: the thresholds
            // belong next to the predictor that produces the numbers, and
            // every other value in this map already arrives display-ready.
            { "confidenceText",    Fmt::confidenceText(transaction.confidence) },
            { "confidenceVariant", Fmt::confidenceVariant(transaction.confidence) },
            { "predictionReason",  transaction.predictionReason },
            // The normalized merchant key this row matched on — shown in the
            // "teach a rule" affordance so the user can see exactly what the
            // rule they're about to create will apply to. Recomputed rather
            // than cached, so editing NAME updates it immediately.
            { "merchantKey",       Predict::LabelNormalizer::normalize(transaction.name,
                                                                        transaction.description).key },
            { "included",          transaction.included },
        });
    }
    return rows;
}

QVariantMap SnapshotImportController::predictionInfo() const
{
    int filled = 0;
    int needsReview = 0;
    for (qsizetype row = 0; row < m_statement.transactions.size(); ++row) {
        const Data::ImportedTransaction& transaction = m_statement.transactions.at(row);
        // Only the rows whose category the predictor filled: a category that
        // the user picked is not "auto-filled" (#53).
        if (transaction.category.isEmpty() || m_userCategoryRows.contains(static_cast<int>(row))) {
            continue;
        }
        ++filled;
        // "Needs review" is deliberately the *same* threshold the badge turns
        // green at (Fmt::confidenceVariant), so the count and the colors can
        // never tell the user two different stories.
        if (transaction.confidence > 0.0 && Fmt::confidenceVariant(transaction.confidence) != QStringLiteral("success")) {
            ++needsReview;
        }
    }

    const int rowCount = static_cast<int>(m_statement.transactions.size());
    QString summary;
    if (!m_predictor.isTrained()) {
        summary = QStringLiteral("No categorized transactions to learn from yet — categorize this "
                                  "statement by hand and the next import will start filling itself in.");
    } else if (rowCount == 0) {
        summary = QStringLiteral("Nothing to categorize.");
    } else {
        summary = QStringLiteral("Auto-filled %1 of %2 row(s) from %3 merchant(s) you've categorized before")
                       .arg(filled)
                       .arg(rowCount)
                       .arg(m_predictor.distinctKeyCount());
        if (m_predictor.ruleCount() > 0) {
            summary += QStringLiteral(" plus %1 of your rules").arg(m_predictor.ruleCount());
        }
        summary += (needsReview > 0)
            ? QStringLiteral(". %1 marked amber/red — worth a look before saving.").arg(needsReview)
            : QStringLiteral(".");
    }

    return QVariantMap{
        { "trained",      m_predictor.isTrained() },
        { "exampleCount", m_predictor.exampleCount() },
        { "keyCount",     m_predictor.distinctKeyCount() },
        { "ruleCount",    m_predictor.ruleCount() },
        { "filledCount",  filled },
        { "reviewCount",  needsReview },
        { "rowCount",     rowCount },
        { "summary",      summary },
    };
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
    m_userEditedRows.clear(); // fresh statement — nobody has decided anything yet
    m_userCategoryRows.clear();
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
        if (success) {
            // Pre-fill categories before the user ever sees the table. Rows
            // the predictor isn't confident enough about are left blank on
            // purpose — see Predict::kMinConfidenceToSuggest.
            retrainPredictor();
            for (int row = 0; row < static_cast<int>(m_statement.transactions.size()); ++row) {
                applyPrediction(row);
            }
        }
    }

    emit statementChanged();
    emit logChanged();

    if (m_lastParseFailed) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Import failed — see the log for details"));
    } else {
        setStatus(StatusMessage::Level::Success, QString("✓  Parsed %1 transaction(s) from %2")
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
    // The user has now decided this row. Drop the predictor's score and
    // explanation (both would describe a category that is no longer there)
    // and remember the row so no later retrain overwrites the decision.
    transaction.confidence = 0.0;
    transaction.predictionReason.clear();
    m_userEditedRows.insert(row);
    m_userCategoryRows.insert(row);
    emit statementChanged();
}

void SnapshotImportController::setRowSubcategory(int row, const QString& subcategory)
{
    if (row < 0 || row >= m_statement.transactions.size()) {
        return;
    }
    // row bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    Data::ImportedTransaction& transaction = m_statement.transactions[row]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    transaction.subcategory = subcategory;
    // Same reasoning as setRowCategory(): this row is the user's now.
    transaction.confidence = 0.0;
    transaction.predictionReason.clear();
    m_userEditedRows.insert(row);
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
    // The label is what the predictor matches on, so correcting it is exactly
    // the case where a re-score is worth doing: fixing a mangled name should
    // immediately produce the suggestion the user was expecting. Rows the
    // user has already categorized by hand are skipped inside
    // applyPrediction(), so this can't undo a decision.
    applyPrediction(row);
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
    // account-number comparisons — accounts don't store those (the app
    // keeps the fingerprint only, see AccountHashing.h), so an account only
    // ever matches if it has actually been linked (see
    // AccountController::linkAccountHash()/addHash()). No hash on the
    // statement, or no account with a matching stored hash → -1, caller
    // shows the manual picker.
    // The fingerprint first, then its form from before #50 (an
    // 8-character BIC, #50): save() replaces that one.
    QStringList candidates = {AccountHashing::compute(m_statement.iban, m_statement.bic)};
    const QString legacy = AccountHashing::legacyCompute(m_statement.iban, m_statement.bic);
    if (!legacy.isEmpty()) {
        candidates.append(legacy);
    }
    if (candidates.first().isEmpty()) {
        return -1;
    }

    for (const QString& targetHash : candidates) {
        for (int i = 0; i < accounts.size(); ++i) {
            const QVariantMap acc = accounts.at(i).toMap();
            const QVariantList hashes = acc.value("hashes").toList();
            for (const QVariant& hashEntry : hashes) {
                if (hashEntry.toMap().value("hash").toString().compare(targetHash, Qt::CaseInsensitive) == 0) {
                    return i;
                }
            }
        }
    }

    return -1;
}

int SnapshotImportController::legacyHashAccountIndex(const QVariantList& accounts) const
{
    const QString legacy = AccountHashing::legacyCompute(m_statement.iban, m_statement.bic);
    if (legacy.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < accounts.size(); ++i) {
        const QVariantList hashes = accounts.at(i).toMap().value(QStringLiteral("hashes")).toList();
        for (const QVariant& hashEntry : hashes) {
            if (hashEntry.toMap().value(QStringLiteral("hash")).toString().compare(legacy, Qt::CaseInsensitive) == 0) {
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
    m_userEditedRows.clear();
    m_userCategoryRows.clear();
    m_predictor.clear(); // rebuilt by the next parseFile() — never stale
    // The report of the last save goes too: after "Import another file", a
    // failed save must not show the report of the earlier import (#39).
    m_lastImportReport.clear();
    emit statementChanged();
    emit logChanged();
    emit reportChanged();
}

// ── Category prediction (see docs/category-prediction.md) ─────────────────────

void SnapshotImportController::retrainPredictor()
{
    // Straight from the database, not from any cached controller state: the
    // user may have recategorized rows on AccountTransactionsPage since this
    // page was opened, and those corrections are supposed to count. Same
    // "write/read straight to Database, no C++-to-C++ singleton coupling"
    // pattern as save().
    const QVector<Data::Transaction> categorized = Database::instance().allCategorizedTransactions();
    const QVector<Data::CategoryRule> rules = Database::instance().loadCategoryRules();
    m_predictor.train(Predict::CategoryPredictor::examplesFromTransactions(categorized), rules);
}

void SnapshotImportController::applyPrediction(int row)
{
    if (row < 0 || row >= m_statement.transactions.size()) {
        return;
    }
    if (m_userEditedRows.contains(row)) {
        return; // the user has decided this row — never second-guess it
    }

    // row bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    Data::ImportedTransaction& transaction = m_statement.transactions[row]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    const Predict::Prediction prediction = m_predictor.predict(
        transaction.name, transaction.description, signedAmountOf(transaction), effectiveDateOf(transaction));

    // An empty prediction clears the row rather than leaving the previous
    // suggestion in place: after a NAME edit the old suggestion was about a
    // different merchant, and a stale suggestion is worse than a blank cell.
    transaction.category = prediction.category;
    transaction.subcategory = prediction.subcategory;
    transaction.confidence = prediction.confidence;
    transaction.predictionReason = prediction.reason;
}

void SnapshotImportController::refreshPredictions()
{
    retrainPredictor();
    for (int row = 0; row < static_cast<int>(m_statement.transactions.size()); ++row) {
        applyPrediction(row);
    }
    emit statementChanged();
}

bool SnapshotImportController::rememberRowAsRule(int row)
{
    if (row < 0 || row >= m_statement.transactions.size()) {
        return false;
    }
    // Copied, not referenced: refreshPredictions() below writes into
    // m_statement.transactions, and a reference into a QList that detaches
    // mid-call would dangle. The status message needs these values afterwards.
    const QString rowName = m_statement.transactions.at(row).name;
    const QString rowDescription = m_statement.transactions.at(row).description;
    const QString rowCategory = m_statement.transactions.at(row).category;
    const QString rowSubcategory = m_statement.transactions.at(row).subcategory;

    if (rowCategory.isEmpty()) {
        setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  Pick a category for this row first, then teach it as a rule"));
        return false;
    }

    // The rule's pattern must be the *normalized* key, never the raw label —
    // the predictor only ever compares normalized keys, so an un-normalized
    // pattern would be stored happily and then never match anything (see
    // Data::CategoryRule::pattern).
    const QString pattern = Predict::LabelNormalizer::normalize(rowName, rowDescription).key;
    if (pattern.isEmpty()) {
        setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  This row's label has nothing distinctive to build a rule from"));
        return false;
    }

    if (Database::instance().addCategoryRule(pattern, rowCategory, rowSubcategory) < 0) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Failed to save the rule — see application log"));
        return false;
    }

    // Re-score immediately so every *other* row matching this merchant in the
    // same statement snaps to the new rule without the user re-importing.
    // The row they taught it from is in m_userEditedRows and stays as-is.
    refreshPredictions();
    setStatus(StatusMessage::Level::Success, QString("✓  From now on, \"%1\" is categorized as %2")
                   .arg(pattern, rowSubcategory.isEmpty()
                                      ? rowCategory
                                      : rowCategory + QStringLiteral(" / ") + rowSubcategory));
    return true;
}

QVariantMap SnapshotImportController::evaluatePredictor()
{
    // Retrain first so the numbers describe the database as it is right now,
    // not whatever was loaded when a statement was last parsed — this is
    // callable with nothing parsed at all.
    retrainPredictor();
    if (m_predictor.exampleCount() == 0) {
        return QVariantMap{ { "available", false } };
    }

    const Predict::Evaluation evaluation = m_predictor.evaluate();
    const auto percentOf = [](int part, int whole) {
        return whole > 0 ? (static_cast<double>(part) * kPercentScale / static_cast<double>(whole)) : 0.0;
    };

    QVariantList byRung;
    for (auto it = evaluation.attemptsByRung.constBegin(); it != evaluation.attemptsByRung.constEnd(); ++it) {
        const int correct = evaluation.correctByRung.value(it.key(), 0);
        byRung.append(QVariantMap{
            { "rung",            it.key() },
            { "attempts",        it.value() },
            { "correct",         correct },
            { "accuracyPercent", percentOf(correct, it.value()) },
        });
    }

    return QVariantMap{
        { "available",               true },
        { "total",                   evaluation.total },
        { "attempted",               evaluation.attempted },
        { "coveragePercent",         percentOf(evaluation.attempted, evaluation.total) },
        { "categoryAccuracyPercent", percentOf(evaluation.categoryCorrect, evaluation.attempted) },
        { "pairAccuracyPercent",     percentOf(evaluation.pairCorrect, evaluation.attempted) },
        { "meanConfidencePercent",   evaluation.meanConfidence * kPercentScale },
        { "byRung",                  byRung },
    };
}

bool SnapshotImportController::save(int accountId, const QString& accountLabel)
{
    if (accountId < 0) {
        setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  Please select an account before saving"));
        return false;
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

    // Auto-link: if this statement has a computable fingerprint, store it
    // for the chosen account (source "auto") unless it is already there —
    // future imports of the same account then auto-match (see § Statement
    // Import / § Account Management in ARCHITECTURE.MD).
    //
    // "Remember the start and end amount from the CSV" (product request) —
    // the statement's own declared balanceStart/balanceEnd become balance
    // checkpoints, same mechanism as manually-added ones (see § Balance
    // Checkpoints), so both this import's own reconciliation-check-1 next
    // time and the Account History line chart have a real anchor even
    // before the user ever opens Manage Accounts to add one by hand.
    //
    // Rows, link and checkpoints go in one SQL transaction: a failure leaves
    // the database as it was, so a retry cannot store a row twice.
    //
    // A row that this account already has (same fingerprint — a re-import,
    // or a statement that overlaps an earlier one) is not saved again (#18).
    // The fingerprints come from the whole statement, before the filter: two
    // identical rows of it are two transactions (occurrence 1 and 2).
    const QStringList fingerprints = TransactionsTable::fingerprints(toSave);
    const QHash<QString, QDate> savedBefore = Database::instance().transactionFingerprints(accountId);
    QVector<Data::Transaction> newRows;
    newRows.reserve(toSave.size());
    int alreadySaved = 0;
    int reusedReferences = 0;
    for (qsizetype i = 0; i < toSave.size(); ++i) {
        const auto saved = savedBefore.constFind(fingerprints.at(i));
        if (saved != savedBefore.cend()) {
            ++alreadySaved;
            if (hasReusedReference(toSave.at(i), saved.value())) {
                ++reusedReferences;
                qWarning().noquote()
                    << QStringLiteral("[SnapshotImportController] row not saved: bank reference %1, amount %2, "
                                      "operation date %3; the saved transaction has %4. See %5")
                           .arg(TransactionsTable::bankReference(toSave.at(i).description))
                           .arg(toSave.at(i).credit - toSave.at(i).debit, 0, 'f', 2)
                           .arg(toSave.at(i).operationDate.toString(Qt::ISODate),
                                saved.value().toString(Qt::ISODate),
                                QLatin1String(TransactionsTable::kReusedReferenceIssueUrl));
            }
            continue;
        }
        Data::Transaction row = toSave.at(i);
        row.fingerprint = fingerprints.at(i);
        newRows.append(row);
    }

    const QString hash = AccountHashing::compute(m_statement.iban, m_statement.bic);
    // The fingerprint from before #50 (an 8-character BIC): replaced by
    // `hash` (#50).
    const QString legacyHash = AccountHashing::legacyCompute(m_statement.iban, m_statement.bic);
    int inserted = 0;
    if (!Database::instance().saveImport(accountId, newRows, hash, statementCheckpointsToAdd(accountId),
                                         &inserted, legacyHash)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not save the import. Nothing was saved — see application log"));
        return false;
    }
    if (inserted != newRows.size()) {
        // Not expected (the filter above used the same fingerprints), but the
        // counts below must tell what the database has.
        qWarning() << "[SnapshotImportController] save: wrote" << inserted << "of" << newRows.size()
                   << "new rows; the others were already saved";
        alreadySaved += static_cast<int>(newRows.size()) - inserted;
    }

    buildImportReport(accountLabel, newRows, reconciliation);
    m_lastImportReport.insert(QStringLiteral("alreadySavedCount"), alreadySaved);
    m_lastImportReport.insert(QStringLiteral("reusedReferenceCount"), reusedReferences);
    m_lastImportReport.insert(QStringLiteral("reusedReferenceUrl"),
                              QLatin1String(TransactionsTable::kReusedReferenceIssueUrl));
    emit reportChanged();

    if (alreadySaved == 0) {
        setStatus(StatusMessage::Level::Success, QString("✓  Saved %1 of %2 transaction(s) to \"%3\"")
                       .arg(inserted)
                       .arg(m_statement.transactions.size())
                       .arg(accountLabel));
    } else if (inserted == 0) {
        setStatus(StatusMessage::Level::Success, QString("✓  Nothing new: the %1 transaction(s) of this statement are already saved in \"%2\"")
                       .arg(alreadySaved)
                       .arg(accountLabel));
    } else {
        setStatus(StatusMessage::Level::Success, QString("✓  Saved %1 new transaction(s) to \"%2\" — %3 already saved, not added again")
                       .arg(inserted)
                       .arg(accountLabel)
                       .arg(alreadySaved));
    }
    return true;
}

QVariantMap SnapshotImportController::checkReconciliation(int accountId) const
{
    QVariantMap result;
    result["available"] = m_hasParsed && !m_lastParseFailed && accountId >= 0;
    if (!result["available"].toBool()) {
        return result;
    }

    // A balance line that could not be read has no date (#44): the checks
    // that need it do not run, instead of counting it as 0.
    const bool hasDeclaredStart = m_statement.balanceStartDate.isValid();
    const bool hasDeclaredEnd = m_statement.balanceEndDate.isValid();
    const bool endAvailable = hasDeclaredStart && hasDeclaredEnd;
    result["hasDeclaredStart"] = hasDeclaredStart;
    result["endAvailable"] = endAvailable;

    // ── Check 1: declared start vs. our own last record before that date ──
    std::optional<Data::AccountBalanceCheckpoint> checkpoint;
    std::optional<Data::Transaction> priorTx;
    if (hasDeclaredStart) {
        checkpoint = Database::instance().latestCheckpointOnOrBefore(accountId, m_statement.balanceStartDate);
        priorTx = Database::instance().latestTransactionOnOrBefore(accountId, m_statement.balanceStartDate);
    }

    bool hasStartReference = false;
    double expectedStart = 0.0;
    QString startSourceLabel;
    QDate startSourceDate;
    // Prefer whichever reference is dated closer to (i.e. more recent
    // before) the statement's own start date — a checkpoint and a
    // transaction can both exist; the most recent one is the better
    // "what did we last know" answer. The date of the transaction is its
    // booking date: its balanceAfter is the balance after that day (#133).
    if (checkpoint.has_value() && (!priorTx.has_value() || checkpoint->date >= Data::bookingDate(*priorTx))) {
        hasStartReference = true;
        expectedStart = checkpoint->balance;
        startSourceLabel = QStringLiteral("balance checkpoint");
        startSourceDate = checkpoint->date;
    } else if (priorTx.has_value()) {
        hasStartReference = true;
        expectedStart = priorTx->balanceAfter;
        startSourceLabel = QStringLiteral("last recorded transaction");
        startSourceDate = Data::bookingDate(*priorTx);
    }

    result["hasStartReference"]   = hasStartReference;
    result["startSourceLabel"]    = startSourceLabel;
    result["startSourceDateText"] = Fmt::dateOrDash(startSourceDate);
    result["startDeclaredText"]   = hasDeclaredStart ? Fmt::money(m_statement.balanceStart) : QStringLiteral("—");
    result["startExpectedText"]   = hasStartReference ? Fmt::money(expectedStart) : QStringLiteral("—");
    // Whole cents (#33): a statement is exact to the cent, and a difference of
    // doubles is not (0.30 - 0.10 - 0.20 is not 0).
    const qint64 startDiffCents = Money::toCents(m_statement.balanceStart) - Money::toCents(expectedStart);
    result["startOk"]       = !hasStartReference || startDiffCents == 0;
    result["startDiffText"] = hasStartReference ? Fmt::money(Money::fromCents(startDiffCents), true) : QStringLiteral("—");

    // ── Check 2: declared start + kept (included) rows vs. declared end ────
    qint64 sumIncludedCents = 0;
    for (const Data::ImportedTransaction& transaction : m_statement.transactions) {
        if (transaction.included) {
            sumIncludedCents += Money::toCents(transaction.credit) - Money::toCents(transaction.debit);
        }
    }
    const qint64 computedEndCents = Money::toCents(m_statement.balanceStart) + sumIncludedCents;
    const qint64 endDiffCents = endAvailable ? Money::toCents(m_statement.balanceEnd) - computedEndCents : 0;

    result["endComputedText"] = endAvailable ? Fmt::money(Money::fromCents(computedEndCents)) : QStringLiteral("—");
    result["endDeclaredText"] = hasDeclaredEnd ? Fmt::money(m_statement.balanceEnd) : QStringLiteral("—");
    result["endDiffText"]     = endAvailable ? Fmt::money(Money::fromCents(endDiffCents), true) : QStringLiteral("—");
    result["endDiffCents"]    = endDiffCents; // for addReconciliationAdjustment()
    // Not checked is not a mismatch: no adjustment, no warning before the save.
    result["endOk"]           = !endAvailable || endDiffCents == 0;

    // ── Rows of this statement that the account already has (#18) ─────────
    // save() skips them; the review screen can tell the user before saving.
    // Same fingerprints as save(): the included rows, in statement order.
    QVector<Data::Transaction> included;
    for (const Data::ImportedTransaction& transaction : m_statement.transactions) {
        if (!transaction.included) {
            continue;
        }
        Data::Transaction row;
        row.confirmationDate = transaction.confirmationDate;
        row.operationDate    = transaction.operationDate;
        row.description      = transaction.description;
        row.debit            = transaction.debit;
        row.credit           = transaction.credit;
        row.balanceAfter     = transaction.balanceAfter;
        included.append(row);
    }
    const QHash<QString, QDate> savedBefore = Database::instance().transactionFingerprints(accountId);
    const QStringList fingerprints = TransactionsTable::fingerprints(included);
    int alreadySaved = 0;
    int reusedReferences = 0;
    for (qsizetype i = 0; i < included.size(); ++i) {
        const auto saved = savedBefore.constFind(fingerprints.at(i));
        if (saved != savedBefore.cend()) {
            ++alreadySaved;
            if (hasReusedReference(included.at(i), saved.value())) {
                ++reusedReferences;
            }
        }
    }
    result["alreadySavedCount"] = alreadySaved;
    result["reusedReferenceCount"] = reusedReferences;

    return result;
}

void SnapshotImportController::addReconciliationAdjustment(int accountId)
{
    const QVariantMap check = checkReconciliation(accountId);
    if (!check.value(QStringLiteral("available")).toBool() || check.value(QStringLiteral("endOk")).toBool()) {
        return; // nothing parsed, or already reconciled — nothing to add
    }

    // Declared end - computed end, in whole cents: the row has the exact gap.
    const double diff = Money::fromCents(check.value(QStringLiteral("endDiffCents")).toLongLong());
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
    // This row's category is set deliberately, not predicted — mark it so a
    // later refreshPredictions() can't relabel a reconciliation adjustment as
    // whatever merchant its synthetic description happens to look like.
    m_userEditedRows.insert(static_cast<int>(m_statement.transactions.size()) - 1);
    m_userCategoryRows.insert(static_cast<int>(m_statement.transactions.size()) - 1);
    emit statementChanged();
    setStatus(StatusMessage::Level::Success, QString("✓  Added a %1 adjustment transaction to match the declared closing balance")
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

QVector<Data::AccountBalanceCheckpoint> SnapshotImportController::statementCheckpointsToAdd(int accountId) const
{
    const QVector<Data::AccountBalanceCheckpoint> existing = Database::instance().loadBalanceCheckpoints(accountId);
    QVector<Data::AccountBalanceCheckpoint> toAdd;
    const auto plan = [&](const QDate& date, double balance, const QString& note) {
        if (!date.isValid()) {
            return;
        }
        const auto sameDate = [&date](const Data::AccountBalanceCheckpoint& checkpoint) {
            return checkpoint.date == date;
        };
        if (std::any_of(existing.cbegin(), existing.cend(), sameDate) ||
            std::any_of(toAdd.cbegin(), toAdd.cend(), sameDate)) {
            return; // already have a checkpoint for this exact date — never overwritten automatically
        }
        Data::AccountBalanceCheckpoint checkpoint;
        checkpoint.accountId = accountId;
        checkpoint.date = date;
        checkpoint.balance = balance;
        checkpoint.note = note;
        toAdd.append(checkpoint);
    };
    plan(m_statement.balanceStartDate, m_statement.balanceStart,
         QStringLiteral("Auto — statement start (%1)").arg(m_sourceFileName));
    plan(m_statement.balanceEndDate, m_statement.balanceEnd,
         QStringLiteral("Auto — statement end (%1)").arg(m_sourceFileName));
    return toAdd;
}

void SnapshotImportController::buildImportReport(const QString& accountLabel, const QVector<Data::Transaction>& saved,
                                                   const QVariantMap& reconciliation)
{
    // Totals in whole cents (#33), given to QML in the unit of the account.
    qint64 totalIncomeCents = 0;
    qint64 totalExpenseCents = 0;
    // Category -> net total (credit - debit); Category -> Subcategory -> net total.
    QMap<QString, qint64> categoryTotals;
    QMap<QString, QMap<QString, qint64>> subcategoryTotals;

    for (const Data::Transaction& transaction : saved) {
        totalIncomeCents  += Money::toCents(transaction.credit);
        totalExpenseCents += Money::toCents(transaction.debit);

        const QString category    = transaction.category.isEmpty() ? QStringLiteral("Uncategorized") : transaction.category;
        const QString subcategory = transaction.subcategory.isEmpty() ? QStringLiteral("—") : transaction.subcategory;
        const qint64 net = Money::toCents(transaction.credit) - Money::toCents(transaction.debit);

        categoryTotals[category] += net;
        subcategoryTotals[category][subcategory] += net;
    }

    QVariantList byCategory;
    for (auto categoryIt = categoryTotals.constBegin(); categoryIt != categoryTotals.constEnd(); ++categoryIt) {
        QVariantList subcategories;
        const QMap<QString, qint64> subMap = subcategoryTotals.value(categoryIt.key());
        for (auto subIt = subMap.constBegin(); subIt != subMap.constEnd(); ++subIt) {
            const double total = Money::fromCents(subIt.value());
            subcategories.append(QVariantMap{
                { "subcategory", subIt.key() },
                { "totalText",   Fmt::money(total, true) },
                { "totalRaw",    total },
            });
        }
        const double total = Money::fromCents(categoryIt.value());
        byCategory.append(QVariantMap{
            { "category",      categoryIt.key() },
            { "totalText",     Fmt::money(total, true) },
            { "totalRaw",      total },
            { "subcategories", subcategories },
        });
    }
    const double totalIncome = Money::fromCents(totalIncomeCents);
    const double totalExpense = Money::fromCents(totalExpenseCents);

    m_lastImportReport = QVariantMap{
        { "accountLabel",      accountLabel },
        { "transactionCount",  saved.size() },
        { "totalIncomeText",   Fmt::money(totalIncome, true) },
        { "totalExpenseText",  Fmt::money(-totalExpense, true) },
        { "netChangeText",     Fmt::money(Money::fromCents(totalIncomeCents - totalExpenseCents), true) },
        { "byCategory",        byCategory },
        { "reconciliation",    reconciliation },
    };
}

void SnapshotImportController::setStatus(StatusMessage::Level level, const QString& msg)
{
    m_status.show(level, msg);
}
