#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── DataEntryController ───────────────────────────────────────────────────────
//
// Backend for qml/pages/DataEntryPage.qml (PROJECT.md page 6 — Data Entry).
// UI pass: forms are rendered but nothing is persisted yet (status says so).
// Category/subcategory data now lives in CategoryController (QML_SINGLETON,
// src/controllers/dataentry/CategoryController.h, same domain folder as this
// class) — DataEntryPage.qml reads categories from there. The old
// keyword-based suggestCategory() was removed per product decision (no more
// auto-fill; categories are picked manually).

class DataEntryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ name, type, iban, bic, number }] — accounts for the monthly snapshot
    // form (mutable). iban/bic/number are MOCK, used by
    // SnapshotImportController to demo matching an imported statement to an
    // existing account via an IBAN+BIC hash (see
    // SnapshotImportController::accountHash()).
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(QStringList accountTypes READ accountTypes CONSTANT)
    // [{ date, label, amount, category }] — mock history
    Q_PROPERTY(QVariantList historyRows READ historyRows CONSTANT)
    // [{ year, income, expenses, saved, rate }] — mock annual summary
    Q_PROPERTY(QVariantList annualRows READ annualRows CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit DataEntryController(QObject* parent = nullptr);

    QVariantList accounts()    const { return m_accounts; }
    QStringList  accountTypes() const { return m_accountTypes; }

    // Account management (mock — not persisted)
    Q_INVOKABLE void addAccount(const QString& name, const QString& type);
    Q_INVOKABLE void removeAccount(int index);
    QVariantList historyRows() const { return m_historyRows; }
    QVariantList annualRows()  const { return m_annualRows; }
    QString statusText()       const { return m_statusText; }

    // UI pass — these only set the status message; persistence comes later.
    Q_INVOKABLE void saveSnapshot();
    Q_INVOKABLE void addTransaction();

signals:
    void accountsChanged();
    void statusChanged();

private:
    void setStatus(const QString& msg);

    QVariantList m_accounts, m_historyRows, m_annualRows;
    QStringList  m_accountTypes;
    QString      m_statusText;
};
