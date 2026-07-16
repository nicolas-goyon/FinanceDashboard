#pragma once

#include <QString>
#include <QDate>
#include <QVector>

// ── Data::ImportedTransaction / Data::ImportedStatement ──────────────────────
//
// Result of running an ImportTemplate::parse() over a bank statement file
// (see src/import/ImportTemplate.h). Header-only, plain data — this is a
// staging area for the "New Monthly Snapshot" import review screen, not a
// DB-backed model yet (see SnapshotImportController — mock save, no schema
// changes this pass; PROJECT.md's "Optional categorized transactions" DB
// wiring is still a TODO in ARCHITECTURE.MD).

namespace Data {

struct ImportedTransaction {
    QDate   confirmationDate;
    QDate   operationDate;
    QString description;
    double  debit{0.0};        // magnitude, 0 if this row is a credit
    double  credit{0.0};       // magnitude, 0 if this row is a debit
    double  balanceAfter{0.0};
    QString category;          // picked manually from CategoryController — empty until set
    QString subcategory;       // picked manually, filtered to the chosen category — empty until set
    bool    included{true};    // unchecked rows are excluded from Save
};

struct ImportedStatement {
    // Header block (see the CSV spec in CheckingCsvTemplateV1)
    QString accountHolderName;
    QString accountName;
    QString clientCode;
    QString accountNumber;
    QString iban;
    QString bic;

    QDate periodFrom;
    QDate periodTo;

    double balanceStart{0.0};
    QDate  balanceStartDate;
    double balanceEnd{0.0};
    QDate  balanceEndDate;

    QVector<ImportedTransaction> transactions;
};

} // namespace Data
