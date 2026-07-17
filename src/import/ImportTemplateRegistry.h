#ifndef FINANCEDASHBOARD_IMPORT_IMPORTTEMPLATEREGISTRY_H
#define FINANCEDASHBOARD_IMPORT_IMPORTTEMPLATEREGISTRY_H

#include <QString>
#include <QVector>

class ImportTemplate;

// ── ImportTemplateRegistry ────────────────────────────────────────────────────
//
// Static list of available ImportTemplate instances. To add a new bank/PDF
// template: implement ImportTemplate (see CheckingCsvTemplateV1 for the CSV
// example), then add one line in the .cpp's all().
//
// Instances are process-lifetime statics — ImportTemplate implementations
// must be stateless (parse() takes the file path and writes its result to
// out-parameters, nothing is cached on the template object itself).

namespace ImportTemplateRegistry {

QVector<const ImportTemplate*> all();
const ImportTemplate* byId(const QString& templateId);

} // namespace ImportTemplateRegistry

#endif // FINANCEDASHBOARD_IMPORT_IMPORTTEMPLATEREGISTRY_H
