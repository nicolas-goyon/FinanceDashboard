#pragma once

#include "models/AssetEntry.h"

#include <QAbstractListModel>
#include <QDate>
#include <QObject>
#include <QVector>
#include <QtQml/qqml.h>

// ── CategoryModel ─────────────────────────────────────────────────────────────
//
// One asset category (Stocks / Coins / Metals / ETFs) of the Entry form.
// Rows hold the user-editable name + value text plus the baseline (the value
// loaded from the DB) used for the diff badge. All parsing, diff % and total
// math is done here in C++ — QML only renders.
//
// Note: edits via setRowValue only emit dataChanged for the diff roles, NOT
// for valueText — this keeps the TextField cursor from jumping while typing.

class CategoryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString totalText READ totalText NOTIFY totalChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        ValueTextRole,
        DiffTextRole,
        DiffStateRole   // "up" | "down" | "none"
    };

    explicit CategoryModel(QString assetType, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    double  total() const;
    QString totalText() const;

    Q_INVOKABLE void addRow();
    Q_INVOKABLE void removeRow(int index);
    Q_INVOKABLE void setRowName(int index, const QString& name);
    Q_INVOKABLE void setRowValue(int index, const QString& valueText);

    // C++-side API used by EntryController
    void fill(const QVector<Data::AssetEntry>& entries);   // sets baselines
    void clearRows();
    QVector<Data::AssetEntry> collect() const;             // legacy collectEntries()

signals:
    void totalChanged();

private:
    struct Row {
        QString name;
        QString valueText;
        double  baseline    = 0.0;
        bool    hasBaseline = false;
    };

    QString diffText(const Row& r) const;
    QString diffState(const Row& r) const;

    QString      m_assetType;
    QVector<Row> m_rows;
};

// ── EntryController ───────────────────────────────────────────────────────────
//
// C++ backend for qml/tabs/EntryTab.qml. Owns the four CategoryModels,
// the selected date, save/load and the status message.

class EntryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(CategoryModel* stocks READ stocks CONSTANT)
    Q_PROPERTY(CategoryModel* coins  READ coins  CONSTANT)
    Q_PROPERTY(CategoryModel* metals READ metals CONSTANT)
    Q_PROPERTY(CategoryModel* etfs   READ etfs   CONSTANT)

    Q_PROPERTY(QString dateText READ dateText NOTIFY dateChanged)   // dd/MM/yyyy
    Q_PROPERTY(int calYear  READ calYear  NOTIFY dateChanged)
    Q_PROPERTY(int calMonth READ calMonth NOTIFY dateChanged)       // 0-based (QML convention)
    Q_PROPERTY(int calDay   READ calDay   NOTIFY dateChanged)

    Q_PROPERTY(QString grandTotalText READ grandTotalText NOTIFY grandTotalChanged)
    Q_PROPERTY(QString statusText     READ statusText     NOTIFY statusChanged)
    Q_PROPERTY(bool    statusIsError  READ statusIsError  NOTIFY statusChanged)

public:
    explicit EntryController(QObject* parent = nullptr);

    CategoryModel* stocks() { return m_stocks; }
    CategoryModel* coins()  { return m_coins;  }
    CategoryModel* metals() { return m_metals; }
    CategoryModel* etfs()   { return m_etfs;   }

    QString dateText() const { return m_date.toString("dd/MM/yyyy"); }
    int calYear()  const { return m_date.year(); }
    int calMonth() const { return m_date.month() - 1; }
    int calDay()   const { return m_date.day(); }

    QString grandTotalText() const;
    QString statusText()    const { return m_statusText; }
    bool    statusIsError() const { return m_statusIsError; }

    // month0 is 0-based (from QML MonthGrid). Ignores invalid/future dates.
    Q_INVOKABLE void setDateYmd(int year, int month0, int day);
    Q_INVOKABLE void save();
    Q_INVOKABLE void clearAll();   // all rows + date back to today

signals:
    void dateChanged();
    void grandTotalChanged();
    void statusChanged();
    void snapshotSaved();

private:
    void loadFor(const QDate& date);   // fill form from DB, or clear rows
    void setStatus(const QString& msg, bool isError = false);

    CategoryModel* m_stocks;
    CategoryModel* m_coins;
    CategoryModel* m_metals;
    CategoryModel* m_etfs;

    QDate   m_date;
    QString m_statusText;
    bool    m_statusIsError = false;
};
