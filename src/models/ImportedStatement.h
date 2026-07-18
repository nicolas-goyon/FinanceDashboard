#ifndef FINANCEDASHBOARD_MODELS_IMPORTEDSTATEMENT_H
#define FINANCEDASHBOARD_MODELS_IMPORTEDSTATEMENT_H

#include <QString>
#include <QDate>
#include <QVector>

// ── Data::ImportedTransaction / Data::ImportedStatement ──────────────────────
//
// Result of running an ImportTemplate::parse() over a bank statement file
// (see src/import/ImportTemplate.h). Header-only, plain data — this is the
// pre-save staging area for the Import Transactions review screen
// (ImportSnapshotPage.qml). Distinct from Data::Transaction
// (models/Transaction.h), the persisted row shape once
// SnapshotImportController::save() actually writes it to the `transactions`
// table via Database::insertTransactions() (see ARCHITECTURE.MD § Statement
// Import — parsing and saving are both real, not mock).

namespace Data {

struct ImportedTransaction {
    QDate   confirmationDate;
    QDate   operationDate;
    QString name;               // auto-extracted from description (every line before the
                                 // "…ref.: …" reference block) — see
                                 // ImportTemplate::parse() implementations. User-editable in
                                 // the review table (SnapshotImportController::setRowName()).
    QString description;
    double  debit{0.0};        // magnitude, 0 if this row is a credit
    double  credit{0.0};       // magnitude, 0 if this row is a debit
    double  balanceAfter{0.0};
    QString category;          // picked manually from CategoryController — empty until set,
                                // or (once a prediction model exists) pre-filled by it
    QString subcategory;       // picked manually, filtered to the chosen category — empty
                                // until set, or pre-filled the same way as category
    double  confidence{0.0};   // 0.0-1.0 — how confident the (future) category-prediction
                                // model is in the `category`/`subcategory` it pre-filled for
                                // this row. Review-screen-only: lets the UI flag low-
                                // confidence rows for the user to double-check before saving.
                                // No model exists yet, so this is always 0.0 for now ("no
                                // confidence, needs review/setting" — not "the model is 0%
                                // sure"). **Deliberately NOT persisted** — save() only ever
                                // writes the resulting category/subcategory to
                                // Data::Transaction, never this field (see ARCHITECTURE.MD §
                                // Statement Import for the full design).
    bool    included{true};    // unchecked rows are excluded from Save
};

struct ImportedStatement {
    // Header block (see the CSV spec in SumeriaCcpCsvExtractionTemplate)
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

#endif // FINANCEDASHBOARD_MODELS_IMPORTEDSTATEMENT_H
