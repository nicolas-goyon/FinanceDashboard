#include "controllers/common/NetWorthScope.h"

#include "db/Database.h"

#include <QHash>

namespace NetWorthScope {

Group groupOf(const Data::Account& account)
{
    if (Data::isInvestment(account)) {
        return Group::Investments;
    }
    return Data::isSavings(account) ? Group::Savings : Group::DayToDay;
}

QVector<ScopeAccount> load(bool all)
{
    Database& db = Database::instance();
    QVector<ScopeAccount> result;
    QHash<int, qsizetype> indexOf;
    for (const Data::Account& account : db.loadAccounts()) {
        if (!all && !account.inNetWorth) {
            continue;
        }
        ScopeAccount scoped;
        scoped.account = account;
        scoped.group = groupOf(account);
        scoped.history.accountId = account.id;
        scoped.history.checkpoints = db.loadBalanceCheckpoints(account.id);
        indexOf.insert(account.id, result.size());
        result.append(scoped);
    }
    for (const Data::Transaction& row : db.allTransactions()) {
        const auto found = indexOf.constFind(row.accountId);
        if (found != indexOf.cend()) {
            result[*found].history.rows.append(row);
        }
    }
    return result;
}

QVector<NetWorthCalc::AccountHistory> histories(const QVector<ScopeAccount>& accounts)
{
    QVector<NetWorthCalc::AccountHistory> result;
    result.reserve(accounts.size());
    for (const ScopeAccount& account : accounts) {
        result.append(account.history);
    }
    return result;
}

} // namespace NetWorthScope
