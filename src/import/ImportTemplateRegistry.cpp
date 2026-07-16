#include "ImportTemplateRegistry.h"
#include "ImportTemplate.h"
#include "CheckingCsvTemplateV1.h"

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

const ImportTemplate* byId(const QString& id)
{
    for (const ImportTemplate* tpl : all())
        if (tpl->id() == id)
            return tpl;
    return nullptr;
}

} // namespace ImportTemplateRegistry
