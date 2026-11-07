#ifndef FINANCEDASHBOARD_MODELS_DEFAULTACCOUNTS_H
#define FINANCEDASHBOARD_MODELS_DEFAULTACCOUNTS_H

#include <QString>
#include <QStringList>
#include <QVector>

// ── Default account types and account categories ─────────────────────────────
//
// The account types and the account categories that a new database gets
// (AccountController::seedDefaults()), that "Reset … to the defaults" writes
// back (Database Management, #239), and that the mock data uses. One list for
// the three, as models/DefaultCategories.h for the transaction categories.
//
// The account category names are translated with the language of the app at
// the time they are written (context "DefaultAccountCategories", see
// Data::accountCategory() in models/Account.h); after that they are data.
// The account types are names of French banking products: they are not
// translated.
namespace Data {

struct DefaultAccountCategory {
    QString name;
    QString color;
};

[[nodiscard]] QStringList defaultAccountTypes();
[[nodiscard]] QVector<DefaultAccountCategory> defaultAccountCategories();

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_DEFAULTACCOUNTS_H
