#ifndef FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_TRANSFERSCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_TRANSFERSCONTROLLER_H

#include "calc/TransferMatcher.h"
#include "utils/StatusMessage.h"

#include <QObject>
#include <QVariantList>
#include <QVector>
#include <QtQml/qqml.h>

// ── TransfersController ──────────────────────────────────────────────────────
//
// Backs qml/pages/InternalTransfersPage.qml (#207). An internal transfer is a
// move of money between two accounts of the net worth (#206): it is not
// income and not spending.
//
//   suggestions: the pairs that TransferMatcher finds in the rows that are
//                not decided yet. The user confirms or refuses each one.
//   transfers:   the confirmed transfers, newest first.
//
// Both lists are read again by refresh() (the page calls it when it opens)
// and after each answer. A row is marked by hand on the transactions page
// (AccountController::setInternalTransfer()).
//
// Data shape:
//   suggestions: [{ debitId, creditId, dateText, amountText, fromAccount,
//                   toAccount, fromName, toName, days }, ...]
//   transfers:   [{ transferId, dateText, amountText, fromAccount, toAccount,
//                   name, complete }, ...]  (complete: both sides are there)
class TransfersController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList suggestions READ suggestions NOTIFY changed)
    Q_PROPERTY(QVariantList transfers READ transfers NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString statusLevel READ statusLevel NOTIFY statusChanged)

public:
    explicit TransfersController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList suggestions() const { return m_suggestions; }
    [[nodiscard]] QVariantList transfers() const { return m_transfers; }
    [[nodiscard]] QString statusText() const { return m_status.text(); }
    [[nodiscard]] QString statusLevel() const { return m_status.variant(); }

    Q_INVOKABLE void refresh();
    // Index into suggestions().
    Q_INVOKABLE bool confirm(int index);
    Q_INVOKABLE bool refuse(int index);
    Q_INVOKABLE int confirmAll();
    // The rows of a confirmed transfer are not a transfer any more.
    Q_INVOKABLE bool cancelTransfer(int transferId);

signals:
    void changed();
    void statusChanged();

private:
    QVector<TransferMatcher::Pair> m_pairs; // the same order as m_suggestions
    QVariantList m_suggestions;
    QVariantList m_transfers;
    StatusMessage m_status;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_TRANSFERSCONTROLLER_H
