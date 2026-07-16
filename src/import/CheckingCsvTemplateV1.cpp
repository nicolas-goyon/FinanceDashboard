#include "CheckingCsvTemplateV1.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace {

using Data::ImportLogEntry;
using Data::LogLevel;

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
        const QChar c = content.at(i);
        rawText += c;

        if (c == QLatin1Char('\n')) {
            ++line;
            if (inQuotes) {
                field += c; // embedded newline inside a quoted (multi-line) field
            } else {
                endRecord();
            }
            continue;
        }

        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < content.size() && content.at(i + 1) == QLatin1Char('"')) {
                    field += QLatin1Char('"'); // escaped quote
                    rawText += content.at(i + 1);
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == QLatin1Char('"')) {
                inQuotes = true;
            } else if (c == QLatin1Char(',')) {
                endField();
            } else {
                field += c;
            }
        }
    }

    // Flush a trailing record that wasn't newline-terminated.
    if (!field.isEmpty() || !fields.isEmpty())
        endRecord();

    return records;
}

// A field can carry an embedded newline (multi-line description) — collapse
// to single-line, whitespace-normalized text for display/matching/logging.
QString normalizeField(QString s)
{
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    s.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return s.simplified();
}

// Single-line, truncated preview of a (possibly multi-line) raw record, for
// the parse log — embedded newlines are shown as "↵" so multi-line
// descriptions are still recognizable as such when debugging.
QString previewRawText(const CsvRecord& rec)
{
    QString s = rec.rawText.trimmed();
    s.replace(QLatin1Char('\n'), QChar(0x21B5)); // ↵
    s.replace(QLatin1Char('\r'), QString());
    if (s.size() > 200)
        s = s.left(200) + QStringLiteral("…");
    return s;
}

// ── Amount parser — tolerant of "1 234.56 €", "1234,56", "-86.40", "€"… ─────
bool parseAmount(QString raw, double& outValue)
{
    raw = raw.trimmed();
    if (raw.isEmpty())
        return false;

    raw.remove(QChar(L'€'));
    raw.remove(QChar(0x202F)); // narrow no-break space (thousands sep)
    raw.remove(QChar(0x00A0)); // no-break space
    raw = raw.simplified();
    // raw.remove(QLatin1Char(' '));

    const int lastComma = raw.lastIndexOf(QLatin1Char(','));
    const int lastDot   = raw.lastIndexOf(QLatin1Char('.'));

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

    bool ok = false;
    const double v = raw.toDouble(&ok);
    if (ok)
        outValue = v;
    return ok;
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

bool CheckingCsvTemplateV1::parse(const QString& filePath,
                                   Data::ImportedStatement& outStatement,
                                   QVector<ImportLogEntry>& outLog) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        outLog.append(ImportLogEntry{ LogLevel::Error, 0,
            QStringLiteral("Could not open file: %1").arg(file.errorString()), QString() });
        return false;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QString content = in.readAll();

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
        if (label.isEmpty())
            continue;

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
            const QRegularExpressionMatch m = kPeriodRe.match(value);
            if (m.hasMatch()) {
                outStatement.periodFrom = parseStatementDate(m.captured(1));
                outStatement.periodTo   = parseStatementDate(m.captured(2));
            } else {
                outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                    QStringLiteral("Could not parse the statement period"), previewRawText(rec) });
            }
        } else {
            const QRegularExpressionMatch m = kBalanceLabelRe.match(label);
            if (m.hasMatch()) {
                double amount = 0.0;
                if (!parseAmount(value, amount)) {
                    outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                        QStringLiteral("Could not parse a statement balance amount"), previewRawText(rec) });
                    continue;
                }
                const QDate date = parseStatementDate(m.captured(1));
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
        if (rec.fields.join(QString()).trimmed().isEmpty())
            continue; // blank line

        if (rec.fields.size() < 6) {
            outLog.append(ImportLogEntry{ LogLevel::Error, rec.startLine,
                QStringLiteral("Expected 6 columns, found %1 — row skipped").arg(rec.fields.size()),
                previewRawText(rec) });
            continue;
        }

        const QDate confirmationDate = parseStatementDateNoYear(rec.fields.at(0), outStatement.periodFrom);
        const QDate operationDate    = parseStatementDateNoYear(rec.fields.at(1), outStatement.periodFrom);
        if (!confirmationDate.isValid() && !operationDate.isValid()) {
            outLog.append(ImportLogEntry{ LogLevel::Error, rec.startLine,
                QStringLiteral("Could not parse either date on this row — row skipped"), previewRawText(rec) });
            continue;
        }

        double debit = 0.0, credit = 0.0, balanceAfter = 0.0;
        const bool hasDebit  = parseAmount(rec.fields.at(3), debit);
        const bool hasCredit = parseAmount(rec.fields.at(4), credit);
        if (!hasDebit && !hasCredit) {
            outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                QStringLiteral("Row has neither a debit nor a credit amount — row skipped"), previewRawText(rec) });
            continue;
        }
        if (hasDebit && hasCredit) {
            outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                QStringLiteral("Row has both a debit and a credit amount — kept both, please check"), previewRawText(rec) });
        }
        if (!parseAmount(rec.fields.at(5), balanceAfter)) {
            outLog.append(ImportLogEntry{ LogLevel::Warning, rec.startLine,
                QStringLiteral("Could not parse the running balance for this row"), previewRawText(rec) });
        }

        Data::ImportedTransaction tx;
        tx.confirmationDate = confirmationDate;
        tx.operationDate    = operationDate.isValid() ? operationDate : confirmationDate;
        tx.description       = normalizeField(rec.fields.at(2)); // may have had embedded newlines
        tx.debit              = std::abs(debit);
        tx.credit             = std::abs(credit);
        tx.balanceAfter       = balanceAfter;
        // No auto-suggestion — category/subcategory are left empty and picked
        // manually in the review table (see CategoryController, removed the
        // old CategoryKeywords keyword pass per product decision).
        outStatement.transactions.append(tx);
        ++parsedCount;
    }

    if (parsedCount == 0) {
        outLog.append(ImportLogEntry{ LogLevel::Warning, 0,
            QStringLiteral("No transaction rows were parsed"), QString() });
    }

    return true;
}
