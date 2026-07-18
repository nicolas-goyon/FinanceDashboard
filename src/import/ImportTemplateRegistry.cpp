#include "ImportTemplateRegistry.h"
#include "ImportTemplate.h"
#include "SumeriaCcpCsvExtractionTemplate.h"

#include <QVector>

namespace ImportTemplateRegistry {

QVector<const ImportTemplate*> all()
{
    // Function-local statics: constructed once, live for the process lifetime.
    static const SumeriaCcpCsvExtractionTemplate kSumeriaCcpCsvExtraction;

    static const QVector<const ImportTemplate*> kAll = {
        &kSumeriaCcpCsvExtraction,
        // Add new templates here as they're implemented.
    };
    return kAll;
}

const ImportTemplate* byId(const QString& templateId)
{
    for (const ImportTemplate* tpl : all()) {
        if (tpl->id() == templateId) {
            return tpl;
        }
    }
    return nullptr;
}

} // namespace ImportTemplateRegistry
