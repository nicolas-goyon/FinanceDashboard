#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── DataEntryController ───────────────────────────────────────────────────────
//
// Backend for qml/pages/DataEntryPage.qml (PROJECT.md page 6 — Data Entry).
// UI pass: forms are rendered but nothing is persisted yet (status says so).
// Category/subcategory data lives in CategoryController (QML_SINGLETON,
// src/controllers/dataentry/CategoryController.h, same domain folder as this
// class) — DataEntryPage.qml reads categories from there. The old
// keyword-based suggestCategory() was removed per product decision (no more
// auto-fill; categories are picked manually).
//
// Accounts used to be a MOCK list owned by this controller (name/type/iban/
// bic/number). They moved out to AccountController (QML_SINGLETON,
// src/controllers/dataentry/AccountController.h) once accounts became a
// real DB-backed, cross-cutting concept (read by this page, the statement
// import page, and their own Manage Accounts page) — same reasoning as why
// categories live in CategoryController rather than here. DataEntryPage.qml
// now reads AccountController.accounts directly; add/edit/remove account UI
// lives on AccountsManagePage.qml (see ARCHITECTURE.MD § Account Management).

class DataEntryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ date, label, amount, category }] — mock history
    Q_PROPERTY(QVariantList historyRows READ historyRows CONSTANT)
    // [{ year, income, expenses, saved, rate }] — mock annual summary
    Q_PROPERTY(QVariantList annualRows READ annualRows CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit DataEntryController(QObject* parent = nullptr);

    QVariantList historyRows() const { return m_historyRows; }
    QVariantList annualRows()  const { return m_annualRows; }
    QString statusText()       const { return m_statusText; }

    // UI pass — these only set the status message; persistence comes later.
    Q_INVOKABLE void saveSnapshot();
    Q_INVOKABLE void addTransaction();

signals:
    void statusChanged();

private:
    void setStatus(const QString& msg);

    QVariantList m_historyRows, m_annualRows;
    QString      m_statusText;
};
