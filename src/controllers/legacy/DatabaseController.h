#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqml.h>

class QSqlTableModel;

// ── DatabaseController ────────────────────────────────────────────────────────
//
// C++ backend for qml/tabs/DatabaseTab.qml.
// Owns two QSqlTableModel subclasses (snapshots + asset_entries) with
// OnManualSubmit edit strategy — edits are buffered until save().
// The `model` property switches with currentTable (0 = snapshots, 1 = entries).

class DatabaseController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QAbstractItemModel* model READ model NOTIFY currentTableChanged)
    Q_PROPERTY(int currentTable READ currentTable WRITE setCurrentTable NOTIFY currentTableChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QString statusText    READ statusText    NOTIFY statusChanged)
    Q_PROPERTY(bool    statusIsError READ statusIsError NOTIFY statusChanged)

public:
    explicit DatabaseController(QObject* parent = nullptr);

    QAbstractItemModel* model() const;
    int  currentTable() const { return m_currentTable; }
    void setCurrentTable(int index);
    int  rowCount() const;

    QString statusText()    const { return m_statusText; }
    bool    statusIsError() const { return m_statusIsError; }

    // Preferred pixel width for a column of the current table.
    Q_INVOKABLE int columnWidth(int column) const;

    Q_INVOKABLE int  addRow();                            // returns new row index
    Q_INVOKABLE void deleteRows(const QVariantList& rows);
    Q_INVOKABLE void save();
    Q_INVOKABLE void revert();
    Q_INVOKABLE void refresh();                           // both tables

signals:
    void currentTableChanged();
    void rowCountChanged();
    void statusChanged();
    void changesSaved();

private:
    QSqlTableModel* currentModel() const;
    void setStatus(const QString& msg, bool isError = false);

    QSqlTableModel* m_snapshotsModel = nullptr;
    QSqlTableModel* m_entriesModel   = nullptr;
    int     m_currentTable  = 0;
    QString m_statusText;
    bool    m_statusIsError = false;
};
