#ifndef FINANCEDASHBOARD_CONTROLLERS_COMMON_NETWORTHSCOPE_H
#define FINANCEDASHBOARD_CONTROLLERS_COMMON_NETWORTHSCOPE_H

#include "calc/NetWorthCalc.h"
#include "models/Account.h"

#include <QVector>

#include <cstdint>

// ── NetWorthScope ────────────────────────────────────────────────────────────
//
// The data of the pages of the net worth (#209–#212): the accounts of the
// net worth (accounts.in_net_worth, #206), each with its rows and its
// balance checkpoints, read from the Database in one pass. The calculations
// are in NetWorthCalc.
namespace NetWorthScope {

enum class Group : std::uint8_t { DayToDay, Savings, Investments };

struct ScopeAccount {
    Data::Account account;
    Group group = Group::DayToDay;
    NetWorthCalc::AccountHistory history;
};

[[nodiscard]] Group groupOf(const Data::Account& account);

// The accounts of the net worth. `all`: every account, also outside it.
[[nodiscard]] QVector<ScopeAccount> load(bool all = false);

// The histories only (for NetWorthCalc::period()).
[[nodiscard]] QVector<NetWorthCalc::AccountHistory> histories(const QVector<ScopeAccount>& accounts);

} // namespace NetWorthScope

#endif // FINANCEDASHBOARD_CONTROLLERS_COMMON_NETWORTHSCOPE_H
