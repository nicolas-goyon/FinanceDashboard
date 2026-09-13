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
    QString category;          // pre-filled by Predict::CategoryPredictor at parse time (see
                                // docs/category-prediction.md), then freely overridable from
                                // CategoryController's list on the review screen
    QString subcategory;       // same, filtered to the chosen category — may stay empty even
                                // when `category` is filled, when the predictor was sure about
                                // the category but not the subcategory
    double  confidence{0.0};   // 0.0-1.0 — how confident Predict::CategoryPredictor is in the
                                // `category`/`subcategory` it pre-filled for this row (see
                                // src/predict/CategoryPredictor.h). Review-screen-only: lets
                                // the UI flag low-confidence rows for the user to double-check
                                // before saving. 0.0 means "not scored" — either nothing
                                // cleared the predictor's minimum, or the user has since set
                                // the category by hand — **not** "the model is 0% sure".
                                // **Deliberately NOT persisted** — save() only ever writes the
                                // resulting category/subcategory to Data::Transaction, never
                                // this field (see ARCHITECTURE.MD § Statement Import for the
                                // full design).
    QString predictionReason;  // one line of plain English explaining where `category` came
                                // from ("Categorized "EDF" as Utilities in 3 of 3 previous
                                // transactions"), surfaced as a tooltip on the confidence
                                // badge. Same review-screen-only, never-persisted status as
                                // `confidence`, and cleared the moment the user overrides the
                                // row by hand — a stale explanation is worse than none.
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
