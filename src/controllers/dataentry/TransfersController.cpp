#include "TransfersController.h"

#include "db/Database.h"
#include "models/Account.h"
#include "utils/Formatter.h"

#include <QHash>
#include <QMap>

#include <algorithm>

namespace {

constexpr int kStatusClearDelayMs = 4000;

struct AccountInfo {
    QString name;
    Fmt::Currency unit = Fmt::Currency::EUR;
};

} // namespace

TransfersController::TransfersController(QObject* parent)
    : QObject(parent)
    , m_status(this, kStatusClearDelayMs, [this]() { emit statusChanged(); })
{
}

void TransfersController::refresh()
{
    Database& db = Database::instance();
    QHash<int, AccountInfo> accounts;
    QSet<int> scope;
    for (const Data::Account& account : db.loadAccounts()) {
        accounts.insert(account.id, AccountInfo{account.name, Fmt::currencyFromCode(account.currency)});
        if (account.inNetWorth) {
            scope.insert(account.id);
        }
    }
    const QVector<Data::Transaction> rows = db.allTransactions();
    QHash<int, const Data::Transaction*> byId;
    for (const Data::Transaction& row : rows) {
        byId.insert(row.id, &row);
    }

    m_pairs = TransferMatcher::find(rows, scope);
    m_suggestions.clear();
    for (const TransferMatcher::Pair& pair : std::as_const(m_pairs)) {
        const Data::Transaction& debit = *byId.value(pair.debitId);
        const Data::Transaction& credit = *byId.value(pair.creditId);
        const AccountInfo from = accounts.value(debit.accountId);
        m_suggestions.append(QVariantMap{
            {"debitId", pair.debitId},
            {"creditId", pair.creditId},
            {"dateText", Fmt::dateOrDash(Data::bookingDate(debit))},
            {"amountText", Fmt::money(debit.debit, from.unit)},
            {"fromAccount", from.name},
            {"toAccount", accounts.value(credit.accountId).name},
            {"fromName", debit.name},
            {"toName", credit.name},
            {"days", pair.days},
        });
    }

    // The confirmed transfers: the rows of each transfer id, the debit first.
    QMap<int, QVector<const Data::Transaction*>> groups;
    for (const Data::Transaction& row : rows) {
        if (Data::isInternalTransfer(row)) {
            groups[row.transferId].append(&row);
        }
    }
    QVector<QVariantMap> transfers;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        const auto debitIt = std::find_if(it->cbegin(), it->cend(), [](const Data::Transaction* r) { return r->debit > 0.0; });
        const auto creditIt = std::find_if(it->cbegin(), it->cend(), [](const Data::Transaction* r) { return r->credit > 0.0; });
        const Data::Transaction* first = debitIt != it->cend() ? *debitIt : it->constFirst();
        const AccountInfo info = accounts.value(first->accountId);
        transfers.append(QVariantMap{
            {"transferId", it.key()},
            {"date", Data::bookingDate(*first)},
            {"dateText", Fmt::dateOrDash(Data::bookingDate(*first))},
            {"amountText", Fmt::money(first->debit > 0.0 ? first->debit : first->credit, info.unit)},
            {"fromAccount", debitIt != it->cend() ? accounts.value((*debitIt)->accountId).name : QStringLiteral("—")},
            {"toAccount", creditIt != it->cend() ? accounts.value((*creditIt)->accountId).name : QStringLiteral("—")},
            {"name", first->name},
            {"complete", debitIt != it->cend() && creditIt != it->cend()},
        });
    }
    std::sort(transfers.begin(), transfers.end(), [](const QVariantMap& a, const QVariantMap& b) {
        return a.value(QStringLiteral("date")).toDate() > b.value(QStringLiteral("date")).toDate();
    });
    m_transfers.clear();
    for (const QVariantMap& transfer : std::as_const(transfers)) {
        m_transfers.append(transfer);
    }
    emit changed();
}

bool TransfersController::confirm(int index)
{
    if (index < 0 || index >= m_pairs.size()) {
        return false;
    }
    const TransferMatcher::Pair pair = m_pairs.at(index);
    if (!Database::instance().confirmTransfer(pair.debitId, pair.creditId)) {
        m_status.show(StatusMessage::Level::Danger, tr("⚠  Could not save the transfer"));
        return false;
    }
    refresh();
    m_status.show(StatusMessage::Level::Success, tr("✓  Transfer confirmed"));
    return true;
}

bool TransfersController::refuse(int index)
{
    if (index < 0 || index >= m_pairs.size()) {
        return false;
    }
    const TransferMatcher::Pair pair = m_pairs.at(index);
    if (!Database::instance().refuseTransfer(pair.debitId, pair.creditId)) {
        m_status.show(StatusMessage::Level::Danger, tr("⚠  Could not save the answer"));
        return false;
    }
    refresh();
    m_status.show(StatusMessage::Level::Success, tr("✓  Not a transfer"));
    return true;
}

int TransfersController::confirmAll()
{
    int confirmed = 0;
    for (const TransferMatcher::Pair& pair : std::as_const(m_pairs)) {
        if (Database::instance().confirmTransfer(pair.debitId, pair.creditId)) {
            ++confirmed;
        }
    }
    refresh();
    m_status.show(confirmed > 0 ? StatusMessage::Level::Success : StatusMessage::Level::Warning,
                  tr("%1  %2 transfer(s) confirmed").arg(confirmed > 0 ? QStringLiteral("✓") : QStringLiteral("⚠"))
                      .arg(confirmed));
    return confirmed;
}

bool TransfersController::cancelTransfer(int transferId)
{
    if (transferId <= 0) {
        return false;
    }
    const auto rows = Database::instance().allTransactions();
    const auto row = std::find_if(rows.cbegin(), rows.cend(),
                                  [transferId](const Data::Transaction& r) { return r.transferId == transferId; });
    if (row == rows.cend() || !Database::instance().setInternalTransfer(row->id, false)) {
        m_status.show(StatusMessage::Level::Danger, tr("⚠  Could not cancel the transfer"));
        return false;
    }
    refresh();
    m_status.show(StatusMessage::Level::Success, tr("✓  Transfer cancelled"));
    return true;
}
