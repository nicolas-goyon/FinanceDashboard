#ifndef FINANCEDASHBOARD_IMPORT_IMPORTTEMPLATE_H
#define FINANCEDASHBOARD_IMPORT_IMPORTTEMPLATE_H

#include "models/ImportedStatement.h"
#include "models/ImportLog.h"

#include <QString>
#include <QVector>

// ── ImportTemplate ────────────────────────────────────────────────────────────
//
// One bank/extract format. Deliberately NOT a declarative/data-driven format
// (decided against that — see ARCHITECTURE.MD § Statement Import): every
// bank export has its own quirks (multi-line headers, quoting, PDF layout,
// …) so each template gets a real C++ implementation of parse() rather than
// a shared generic reader. New formats = new subclass + registration in
// ImportTemplateRegistry, not a config file.
//
// parse() must never throw and must never crash on malformed input — bad
// rows are recorded as Data::ImportLogEntry (Warning/Error) in `log` and
// skipped, so the user can still review whatever DID parse and see what
// went wrong via the "console/log" popup on the import page.

class ImportTemplate
{
public:
    ImportTemplate() = default;
    virtual ~ImportTemplate() = default;

    // Polymorphic base — copying/moving through the base pointer would slice
    // the derived template, so disable it explicitly (Core Guidelines C.67).
    ImportTemplate(const ImportTemplate&) = delete;
    ImportTemplate& operator=(const ImportTemplate&) = delete;
    ImportTemplate(ImportTemplate&&) = delete;
    ImportTemplate& operator=(ImportTemplate&&) = delete;

    // Stable key (used by SnapshotImportController::parseFile, never shown to the user).
    [[nodiscard]] virtual QString id() const = 0;

    // Shown in the template picker ComboBox.
    [[nodiscard]] virtual QString displayName() const = 0;

    // QtQuick.Dialogs FileDialog nameFilters entry, e.g. "CSV files (*.csv)".
    [[nodiscard]] virtual QString fileFilter() const = 0;

    // Parse `filePath` into `outStatement`, appending any issues to `outLog`.
    // Returns false only when parsing failed so badly that outStatement is
    // not usable at all (e.g. file could not be opened, header unrecognized);
    // partial row failures are logged but do not make parse() return false.
    virtual bool parse(const QString& filePath,
                        Data::ImportedStatement& outStatement,
                        QVector<Data::ImportLogEntry>& outLog) const = 0;
};

#endif // FINANCEDASHBOARD_IMPORT_IMPORTTEMPLATE_H
