#pragma once

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
    QString description;
    double  debit{0.0};        // magnitude, 0 if this row is a credit
    double  credit{0.0};       // magnitude, 0 if this row is a debit
    double  balanceAfter{0.0};
    QString category;
    QString subcategory;
    QString sourceTemplate;    // ImportTemplate::id() this row came from, or "" if manual
};

} // namespace Data
