#include "SumeriaCcpCsvExtractionTemplate.h"
#include "models/ImportLog.h"
#include "models/ImportedStatement.h"

#include <QChar>
#include <QDate>
#include <QFile>
#include <QLatin1Char>
#include <QLatin1String>
#include <QRegularExpression>
#include <QString>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>
#include <QVector>
#include <cmath>

namespace {

using Data::ImportLogEntry;
using Data::LogLevel;

constexpr char16_t kDownwardsArrowWithCorner = 0x21B5; // ↵ (embedded-newline marker in log previews)
constexpr char16_t kNarrowNoBreakSpace = 0x202F;       // narrow no-break space (thousands sep)
constexpr char16_t kNoBreakSpace = 0x00A0;             // no-break space
constexpr int kPreviewMaxChars = 200;                  // parse-log preview truncation length

// Transaction row column layout (see the format spec in this file's header comment).
constexpr int kExpectedColumnCount = 6;
constexpr int kColConfirmationDate = 0;
constexpr int kColOperationDate = 1;
constexpr int kColDescription = 2;
constexpr int kColDebit = 3;
constexpr int kColCredit = 4;
constexpr int kColBalanceAfter = 5;

// ── One logical CSV record (row), possibly spanning several physical lines
//    when a quoted field — e.g. "Description and identifier" — contains
//    embedded newlines. See parseCsvRecords() below: we CANNOT split the
//    file into physical lines first and parse each independently, because a
//    quoted field's embedded '\n' would then land in the wrong "row".
struct CsvRecord {
    QStringList fields;
    int     startLine{1};  // 1-based physical line where this record began
    QString rawText;       // original text (may itself contain embedded newlines)
};

// ── Tokenizes the WHOLE file content into logical CSV records in one pass,
//    tracking quote state across the entire text (not line by line). A
//    newline only ends a record when it occurs outside an open quote;
//    inside quotes it becomes part of the field's text (multi-line
//    description). "" inside a quoted field is the standard escaped-quote.
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — quote-aware,
// whole-file tokenizing genuinely needs this much branching in one pass (see
// ARCHITECTURE.MD § Statement Import for why it isn't line-by-line); splitting
// it up risks breaking the embedded-newline/escaped-quote handling described
// there. Left as-is per product decision rather than refactored blind.
QVector<CsvRecord> parseCsvRecords(const QString& content)
{
    QVector<CsvRecord> records;
    QStringList fields;
    QString field;
    QString rawText;
    bool inQuotes = false;
    int line = 1;
    int recordStartLine = 1;

    auto endField = [&]() {
        fields << field;
        field.clear();
    };
    auto endRecord = [&]() {
        endField();
        records.append(CsvRecord{ fields, recordStartLine, rawText });
        fields.clear();
        rawText.clear();
        recordStartLine = line;
    };

    for (int i = 0; i < content.size(); ++i) {
        const QChar currentChar = content.at(i);
        rawText += currentChar;

        if (currentChar == QLatin1Char('\n')) {
            ++line;
            if (inQuotes) {
                field += currentChar; // embedded newline inside a quoted (multi-line) field
            } else {
                endRecord();
            }
            continue;
        }

        if (inQuotes) {
            if (currentChar == QLatin1Char('"')) {
                if (i + 1 < content.size() && content.at(i + 1) == QLatin1Char('"')) {
                    field += QLatin1Char('"'); // escaped quote
                    rawText += content.at(i + 1);
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += currentChar;
            }
        } else {
            if (currentChar == QLatin1Char('"')) {
                inQuotes = true;
            } else if (currentChar == QLatin1Char(',')) {
                endField();
            } else {
                field += currentChar;
            }
        }
    }

    // Flush a trailing record that wasn't newline-terminated.
    if (!field.isEmpty() || !fields.isEmpty()) {
        endRecord();
    }

    return records;
}

// A field can carry an embedded newline (multi-line description) — collapse
// to single-line, whitespace-normalized text for display/matching/logging.
QString normalizeField(QString text)
{
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return text.simplified();
}

// A line that starts the reference block ("External ref.: …", "Internal
// ref.: …") — matched on the "ref.:" (or "ref :" / "ref:") suffix alone
// rather than hardcoding "External"/"Internal", since that's the part both
// known examples share and a third ref label isn't unlikely.
bool isReferenceLine(const QString& line)
{
    static const QRegularExpression kRefLineRe(
        QStringLiteral("\\bref\\s*[.:]"), QRegularExpression::CaseInsensitiveOption);
    return kRefLineRe.match(line).hasMatch();
}

// ── Transaction "name" extraction ────────────────────────────────────────
// Per product decision: the description field is multi-line, and how much
// of it is "the name" varies by transaction type — two real examples seen
// so far (the values below are made up):
//
//   Card payment 1234: SAMPLE STORE
//   External ref.: 123456789012345
//   Internal ref.: a1b2c3d4-0000-4000-8000-000000000001
//
//   Payment received from Sample Broker
//   "Dividend - Sample Corp"
//   Internal ref.: 0123456789ABCDEF0123456789ABCDEF
//
// ("External ref." isn't always present.) Both examples share the same
// shape: one or more free-text lines, followed by a block of "…ref.: …"
// lines that are never part of the name. So the rule is: take every line
// from the start of the (raw, pre-normalizeField) description UP TO the
// first line that looks like a reference line, and join them with a single
// space — one line for the card-payment example, two for the dividend one,
// with no per-transaction-type special-casing needed. If a future real
// example needs different handling, verify against a real file first (see
// ARCHITECTURE.MD § Statement Import / this file's header note) before
// changing this rule. The full description (all lines, refs included) is
// unaffected — still stored via normalizeField() as before.
//
// Defensively collapses any doubled CSV-escaped quote ("" -> ") that
// might still be present in the extracted name — parseCsvRecords() already
// unescapes "" -> " while a field is being tokenized, so this is normally a
// no-op, but the dividend example above is exactly the kind of nested-quote
// content this is here to guard against if a real file ever parses
// differently than expected.
QString extractTransactionName(const QString& rawDescription)
{
    const QStringList lines = rawDescription.split(QLatin1Char('\n'));
    QStringList nameLines;
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (isReferenceLine(line)) {
            break; // this line and everything after it is the reference block, never the name
        }
        nameLines << line;
    }

    QString name = nameLines.join(QLatin1Char(' '));
    name.replace(QStringLiteral("\"\""), QStringLiteral("\""));
    return name.simplified();
}

// Single-line, truncated preview of a (possibly multi-line) raw record, for
// the parse log — embedded newlines are shown as "↵" so multi-line
// descriptions are still recognizable as such when debugging.
QString previewRawText(const CsvRecord& rec)
{
    QString text = rec.rawText.trimmed();
    text.replace(QLatin1Char('\n'), QChar(kDownwardsArrowWithCorner));
    text.replace(QLatin1Char('\r'), QString());
    if (text.size() > kPreviewMaxChars) {
        text = text.left(kPreviewMaxChars) + QStringLiteral("…");
    }
    return text;
}

// ── Amount parser — tolerant of "1 234.56 €", "1234,56", "-86.40", "€"… ─────
bool parseAmount(QString raw, double& outValue)
{
    raw = raw.trimmed();
    if (raw.isEmpty()) {
        return false;
    }

    raw.remove(QChar(L'€'));
    raw.remove(QChar(kNarrowNoBreakSpace));
    raw.remove(QChar(kNoBreakSpace));
    raw = raw.simplified();
    // raw.remove(QLatin1Char(' '));

    const int lastComma = static_cast<int>(raw.lastIndexOf(QLatin1Char(',')));
    const int lastDot   = static_cast<int>(raw.lastIndexOf(QLatin1Char('.')));

    if (lastComma >= 0 && lastDot >= 0) {
        // Both present — the later one is the decimal separator, the earlier
        // one is a thousands separator to strip.
        if (lastComma > lastDot) {
            raw.remove(QLatin1Char('.'));
            raw.replace(QLatin1Char(','), QLatin1Char('.'));
        } else {
            raw.remove(QLatin1Char(','));
        }
    } else if (lastComma >= 0) {
        raw.replace(QLatin1Char(','), QLatin1Char('.'));
    }
    // else: only dot or no separator at all — already toDouble()-ready.

    bool success = false;
    const double parsed = raw.toDouble(&success);
    if (success) {
        outValue = parsed;
    }
    return success;
}

QDate parseStatementDate(const QString& raw)
{
    // Format given verbatim in the spec: MM/DD/YYYY.
    return QDate::fromString(raw.trimmed(), QStringLiteral("MM/dd/yyyy"));
}

QDate parseStatementDateNoYear(const QString& raw, QDate refYear)
{
    // Format given verbatim in the spec: MM/DD/YYYY.
    QDate baseDate = QDate::fromString(raw.trimmed(), QStringLiteral("MM/dd"));
    baseDate.setDate(refYear.year(),baseDate.month(),baseDate.day());
    return baseDate;
}

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — the row-level
// parsing loop (multi-line records, metadata lines, per-field validation,
// per-row log entries) is documented in detail in ARCHITECTURE.MD § Statement
// Import; splitting it up risks breaking one of the subtle cases described
// there (embedded newlines, "" escaping, MM/dd/yyyy dates). Left as-is per
// product decision rather than refactored blind.
bool SumeriaCcpCsvExtractionTemplate::parse(const QString& filePath,
                                             Data::ImportedStatement& outStatement,
                                             QVector<ImportLogEntry>& outLog) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        outLog.append(ImportLogEntry{ LogLevel::Error, 0,
            QStringLiteral("Could not open file: %1").arg(file.errorString()), QString() });
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString content = stream.readAll();

    if (content.trimmed().isEmpty()) {
        outLog.append(ImportLogEntry{ LogLevel::Error, 0,
            QStringLiteral("File is empty"), QString() });
        return false;
    }

    // Whole-file, quote-aware tokenization — NOT line-by-line. A physical
    // readLine() pass would split a quoted multi-line description across
    // several bogus "rows" and corrupt everything after it.
    const QVector<CsvRecord> records = parseCsvRecords(content);

    if (records.isEmpty()) {
        outLog.append(ImportLogEntry{ LogLevel::Error, 0,
            QStringLiteral("File is empty"), QString() });
        return false;
    }

    // ── 1. Locate the transaction header row (record containing both
    //    "confirmation date" and "operation date", case-insensitive) ─────────
    int headerRecordIndex = -1;
    for (int i = 0; i < records.size(); ++i) {
        const QString lower = records.at(i).fields.join(QLatin1Char(',')).toLower();
        if (lower.contains(QLatin1String("confirmation date")) &&
            lower.contains(QLatin1String("operation date"))) {
            headerRecordIndex = i;
            break;
        }
    }

    if (headerRecordIndex < 0) {
        outLog.append(ImportLogEntry{ LogLevel::Error, 0,
            QStringLiteral("Could not find the transaction header row "
                            "(expected a line mentioning \"Confirmation date\" "
                            "and \"Operation date\") — is this the right template?"),
            QString() });
        return false;
    }

    // ── 2. Parse the metadata block (everything before the header row) ─────
    static const QRegularExpression kPeriodRe(
        QStringLiteral("From\\s+(\\d{2}/\\d{2}/\\d{4})\\s+to\\s+(\\d{2}/\\d{2}/\\d{4})"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kBalanceLabelRe(
        QStringLiteral("Account balance on\\s+(\\d{2}/\\d{2}/\\d{4})"),
        QRegularExpression::CaseInsensitiveOption);

    int balanceLinesSeen = 0;

    for (int i = 0; i < headerRecordIndex; ++i) {
        const CsvRecord& rec = records.at(i);
        const QString label = normalizeField(rec.fields.value(0));
        if (label.isEmpty()) {
            continue;
        }

        const QString value = rec.fields.size() > 1
                             ? normalizeField(rec.fields.mid(1).join(QLatin1Char(',')))
                             : QString();
        const QString labelLower = label.toLower();

        // Accent-insensitive-ish match: accept both "Prénom" and "Prenom".
        if (labelLower.contains(QLatin1String("prénom")) || labelLower.contains(QLatin1String("prenom"))) {
            outStatement.accountHolderName = value;
        } else if (labelLower == QLatin1String("account name")) {
            outStatement.accountName = value;
        } else if (labelLower == QLatin1String("client code")) {
            outStatement.clientCode = value;
        } else if (labelLower == QLatin1String("account number")) {
            outStatement.accountNumber = value;
        } else if (labelLower == QLatin1String("iban")) {
            outStatement.iban = QString(value).remove(QLatin1Char(' '));
        } else if (labelLower == QLatin1String("bic")) {
            outStatement.bic = value;
        } else if (labelLower == QLatin1String("period")) {
            const QRegularExpressionMatch match = kPeriodRe.match(value);
            if (match.hasMatch()) {
                outStatement.periodFrom = parseStatementDate(match.captured(1));
                outStatement.periodTo   = parseStatementDate(match.captured(2));
            } else {
                outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                    QStringLiteral("Could not parse the statement period"), previewRawText(rec) });
            }
        } else {
            const QRegularExpressionMatch match = kBalanceLabelRe.match(label);
            if (match.hasMatch()) {
                double amount = 0.0;
                if (!parseAmount(value, amount)) {
                    outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                        QStringLiteral("Could not parse a statement balance amount"), previewRawText(rec) });
                    continue;
                }
                const QDate date = parseStatementDate(match.captured(1));
                if (balanceLinesSeen == 0) {
                    outStatement.balanceStartDate = date;
                    outStatement.balanceStart = amount;
                } else if (balanceLinesSeen == 1) {
                    outStatement.balanceEndDate = date;
                    outStatement.balanceEnd = amount;
                } else {
                    outLog.append(ImportLogEntry{ LogLevel::Info, rec.startLine,
                        QStringLiteral("Extra \"Account balance on\" line ignored (only 2 expected)"), previewRawText(rec) });
                }
                ++balanceLinesSeen;
            }
            // Unrecognized metadata lines are silently ignored — not every
            // export necessarily has exactly this header set.
        }
    }

    if (balanceLinesSeen < 2) {
        outLog.append(ImportLogEntry{ LogLevel::Warning, 0,
            QStringLiteral("Expected 2 \"Account balance on\" lines, found %1").arg(balanceLinesSeen),
            QString() });
    }

    // ── 3. Parse transaction rows (everything after the header row) ────────
    int parsedCount = 0;
    for (int i = headerRecordIndex + 1; i < records.size(); ++i) {
        const CsvRecord& rec = records.at(i);
        if (rec.fields.join(QString()).trimmed().isEmpty()) {
            continue; // blank line
        }

        if (rec.fields.size() < kExpectedColumnCount) {
            outLog.append(ImportLogEntry{ LogLevel::Error, rec.startLine,
                QStringLiteral("Expected 6 columns, found %1 — row skipped").arg(rec.fields.size()),
                previewRawText(rec) });
            continue;
        }

        const QDate confirmationDate = parseStatementDateNoYear(rec.fields.at(kColConfirmationDate), outStatement.periodFrom);
        const QDate operationDate    = parseStatementDateNoYear(rec.fields.at(kColOperationDate), outStatement.periodFrom);
        if (!confirmationDate.isValid() && !operationDate.isValid()) {
            outLog.append(ImportLogEntry{ LogLevel::Error, rec.startLine,
                QStringLiteral("Could not parse either date on this row — row skipped"), previewRawText(rec) });
            continue;
        }

        double debit       = 0.0;
        double credit      = 0.0;
        double balanceAfter = 0.0;
        const bool hasDebit  = parseAmount(rec.fields.at(kColDebit), debit);
        const bool hasCredit = parseAmount(rec.fields.at(kColCredit), credit);
        if (!hasDebit && !hasCredit) {
            outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                QStringLiteral("Row has neither a debit nor a credit amount — row skipped"), previewRawText(rec) });
            continue;
        }
        if (hasDebit && hasCredit) {
            outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                QStringLiteral("Row has both a debit and a credit amount — kept both, please check"), previewRawText(rec) });
        }
        if (!parseAmount(rec.fields.at(kColBalanceAfter), balanceAfter)) {
            outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                QStringLiteral("Could not parse the running balance for this row"), previewRawText(rec) });
        }

        Data::ImportedTransaction transaction;
        transaction.confirmationDate = confirmationDate;
        transaction.operationDate    = operationDate.isValid() ? operationDate : confirmationDate;
        transaction.name              = extractTransactionName(rec.fields.at(kColDescription)); // lines before the ref block, raw (pre-normalize)
        transaction.description       = normalizeField(rec.fields.at(kColDescription)); // may have had embedded newlines
        transaction.debit              = std::abs(debit);
        transaction.credit             = std::abs(credit);
        transaction.balanceAfter       = balanceAfter;
        // No auto-suggestion — category/subcategory are left empty and picked
        // manually in the review table (see CategoryController, removed the
        // old CategoryKeywords keyword pass per product decision).
        // confidence stays at its struct default (0.0) — no category-
        // prediction model exists yet to set it to anything else.
        outStatement.transactions.append(transaction);
        ++parsedCount;
    }

    if (parsedCount == 0) {
        outLog.append(ImportLogEntry{ LogLevel::Warning, 0,
            QStringLiteral("No transaction rows were parsed"), QString() });
    }

    return true;
}
