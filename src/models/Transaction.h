#ifndef FINANCEDASHBOARD_MODELS_TRANSACTION_H
#define FINANCEDASHBOARD_MODELS_TRANSACTION_H

#include <QString>
#include <QDate>

// ── Data::Transaction ────────────────────────────────────────────────────────
//
// A persisted, categorized transaction linked to an account (`transactions`
// table — see db/tables/TransactionsTable.h). Distinct from
// Data::ImportedTransaction (models/ImportedStatement.h), which is the
// pre-save staging row used by the statement-import review screen — this is
// the row shape once SnapshotImportController::save() actually writes it
// through Database::insertTransactions() (see ARCHITECTURE.MD § Statement
// Import / § Account Management).

namespace Data {

struct Transaction {
    int     id = -1;
    int     accountId = -1;
    QDate   confirmationDate;
    QDate   operationDate;
    QString name;               // short label extracted from the description at import time
                                 // (or edited by the user in the review table) — see
                                 // Data::ImportedTransaction::name / ARCHITECTURE.MD §
                                 // Statement Import.
    QString description;
    double  debit{0.0};        // magnitude, 0 if this row is a credit
    double  credit{0.0};       // magnitude, 0 if this row is a debit
    double  balanceAfter{0.0};
    QString category;
    QString subcategory;
    QString sourceTemplate;    // ImportTemplate::id() this row came from, or "" if manual
    // No `confidence` field here on purpose — the (future) category-
    // prediction model's per-row confidence is a review-screen-only concept
    // (see Data::ImportedTransaction::confidence), never written to the
    // `transactions` table. Once the user reviews/fixes a row and saves,
    // only the resulting `category`/`subcategory` are persisted — see
    // ARCHITECTURE.MD § Statement Import.
};

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_TRANSACTION_H
