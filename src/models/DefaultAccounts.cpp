#include "models/DefaultAccounts.h"

#include "models/Account.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Data {

QStringList defaultAccountTypes()
{
    // The banking and financial products. models/Account.h (isInvestment(),
    // isSavings()) knows some of these names.
    return {
        QStringLiteral("CCP"), QStringLiteral("Livret A"), QStringLiteral("LDDS"), QStringLiteral("PEL"),
        QStringLiteral("Assurance Vie"), QStringLiteral("PEA"), QStringLiteral("CTO"), QStringLiteral("Other"),
    };
}

QVector<DefaultAccountCategory> defaultAccountCategories()
{
    // The 4 usage buckets: day-to-day, savings, investments, urgent savings.
    // QT_TRANSLATE_NOOP marks the names for lupdate; accountCategory()
    // translates them.
    return {
        { accountCategory(QT_TRANSLATE_NOOP("DefaultAccountCategories", "Day-to-day")),     QStringLiteral("#89b4fa") },
        { accountCategory(QT_TRANSLATE_NOOP("DefaultAccountCategories", "Savings")),        QStringLiteral("#a6e3a1") },
        { accountCategory(QT_TRANSLATE_NOOP("DefaultAccountCategories", "Investments")),    QStringLiteral("#cba6f7") },
        { accountCategory(QT_TRANSLATE_NOOP("DefaultAccountCategories", "Urgent savings")), QStringLiteral("#f9e2af") },
    };
}

} // namespace Data
