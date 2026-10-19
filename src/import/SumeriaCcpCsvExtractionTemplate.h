#ifndef FINANCEDASHBOARD_IMPORT_SUMERIACCPCSVEXTRACTIONTEMPLATE_H
#define FINANCEDASHBOARD_IMPORT_SUMERIACCPCSVEXTRACTIONTEMPLATE_H

#include "ImportTemplate.h"

// ── SumeriaCcpCsvExtractionTemplate ───────────────────────────────────────────
//
// First supported bank-export format: a Sumeria CCP checking-account CSV
// statement whose layout is (as given, verbatim, when this template was
// written):
//
//   Prénom Nom,{Name} {Surname}
//   Account name,{Account name}
//   Client code,{Client Code}
//   Account number,{Account number}
//   IBAN,{IBAN}
//   BIC,{BIC}
//   Period,From {MM/DD/YYYY} to  {MM/DD/YYYY}
//   Account balance on {MM/DD/YYYY},{XXX.XX} €
//   Account balance on {MM/DD/YYYY},{XXX.XX} €
//   ¹Confirmation date/ value,²Operation date,Description and identifier,Debit (-),Credit (+),Balance
//   {ConfirmationDate},{OperationDate},"{DescriptionFormated}",{Debit},{Credit},{BalanceAfterOperation}
//   ...
//
// Header fields are located by label match (not fixed line numbers) so a
// stray blank line doesn't break parsing. See .cpp for row-level error
// handling — malformed rows are logged and skipped rather than aborting
// the whole import.
//
// The row dates are "MM/dd", without a year: each one gets the year that
// puts it in the statement period (from 31 days before its start to its
// end), so the "Period" line is required.
//
// IMPORTANT: "{DescriptionFormated}" is a quoted CSV field that can contain
// EMBEDDED NEWLINES (a genuinely multi-line description). The whole file is
// therefore tokenized in one pass tracking quote state (parseCsvRecords() in
// the .cpp) instead of being split into physical lines first — a
// readLine()-per-row approach would break a multi-line description into
// several bogus rows and corrupt every row after it. Embedded newlines are
// collapsed to spaces in the final Data::ImportedTransaction::description.

class SumeriaCcpCsvExtractionTemplate final : public ImportTemplate
{
public:
    [[nodiscard]] QString id() const override { return QStringLiteral("sumeria_ccp_csv_extraction"); }
    [[nodiscard]] QString displayName() const override { return QStringLiteral("Sumeria CCP — CSV Statement"); }
    [[nodiscard]] QString fileFilter() const override { return QStringLiteral("CSV files (*.csv)"); }

    bool parse(const QString& filePath,
               Data::ImportedStatement& outStatement,
               QVector<Data::ImportLogEntry>& outLog) const override;
};

#endif // FINANCEDASHBOARD_IMPORT_SUMERIACCPCSVEXTRACTIONTEMPLATE_H
