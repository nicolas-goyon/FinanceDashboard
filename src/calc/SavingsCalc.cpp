#include "calc/SavingsCalc.h"

#include <QRegularExpression>

#include <array>
#include <utility>

namespace SavingsCalc {

qint64 ceilingCents(const QString& type)
{
    static const std::array<std::pair<const char*, qint64>, 6> kCeilings = {{
        {"Livret A", 2295000},
        {"LDDS", 1200000},
        {"LEP", 1000000},
        {"PEL", 6120000},
        {"CEL", 1530000},
        {"Livret Jeune", 160000},
    }};
    for (const auto& [name, cents] : kCeilings) {
        if (type.compare(QLatin1String(name), Qt::CaseInsensitive) == 0) {
            return cents;
        }
    }
    return 0;
}

bool isInterest(const Data::Transaction& row)
{
    if (row.credit <= 0.0) {
        return false;
    }
    static const QRegularExpression kInterest(QStringLiteral("\\b(interest|int\u00e9r\u00eats?|interets?)\\b"),
                                              QRegularExpression::CaseInsensitiveOption
                                                  | QRegularExpression::UseUnicodePropertiesOption);
    return kInterest.match(row.category).hasMatch() || kInterest.match(row.subcategory).hasMatch()
           || kInterest.match(row.name).hasMatch() || kInterest.match(row.description).hasMatch();
}

} // namespace SavingsCalc
