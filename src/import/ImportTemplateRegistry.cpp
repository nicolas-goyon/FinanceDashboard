#include "ImportTemplateRegistry.h"
#include "ImportTemplate.h"
#include "CheckingCsvTemplateV1.h"

#include <QVector>

namespace ImportTemplateRegistry {

QVector<const ImportTemplate*> all()
{
    // Function-local statics: constructed once, live for the process lifetime.
    static const CheckingCsvTemplateV1 kCheckingCsvV1;

    static const QVector<const ImportTemplate*> kAll = {
        &kCheckingCsvV1,
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
