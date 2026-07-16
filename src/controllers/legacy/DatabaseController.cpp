#include "DatabaseController.h"
#include "db/Database.h"

#include <QSqlError>
#include <QSqlTableModel>
#include <QTimer>

#include <algorithm>

// ── Private model ─────────────────────────────────────────────────────────────
//
// - Column 0 (integer primary key) is read-only.
// - QML TableView's edit delegate writes via the "display" role name, which
//   maps to Qt::DisplayRole; QSqlTableModel only accepts Qt::EditRole, so we
//   remap it here.

class DbTableModel : public QSqlTableModel
{
    Q_OBJECT
public:
    using QSqlTableModel::QSqlTableModel;

    Qt::ItemFlags flags(const QModelIndex& idx) const override
    {
        const auto base = QSqlTableModel::flags(idx);
        if (idx.column() == 0)
            return base & ~Qt::ItemIsEditable;
        return base;
    }

    bool setData(const QModelIndex& idx, const QVariant& value,
                 int role = Qt::EditRole) override
    {
        if (role == Qt::DisplayRole)
            role = Qt::EditRole;
        return QSqlTableModel::setData(idx, value, role);
    }
};

// ── DatabaseController ────────────────────────────────────────────────────────

DatabaseController::DatabaseController(QObject* parent)
    : QObject(parent)
{
    QSqlDatabase db = Database::instance().sqlDatabase();

    auto* snapshots = new DbTableModel(this, db);
    snapshots->setTable("snapshots");
    snapshots->setEditStrategy(QSqlTableModel::OnManualSubmit);
    snapshots->select();
    m_snapshotsModel = snapshots;

    auto* entries = new DbTableModel(this, db);
    entries->setTable("asset_entries");
    entries->setEditStrategy(QSqlTableModel::OnManualSubmit);
    entries->select();
    m_entriesModel = entries;

    for (QSqlTableModel* m : { m_snapshotsModel, m_entriesModel }) {
        connect(m, &QAbstractItemModel::rowsInserted, this, &DatabaseController::rowCountChanged);
        connect(m, &QAbstractItemModel::rowsRemoved,  this, &DatabaseController::rowCountChanged);
        connect(m, &QAbstractItemModel::modelReset,   this, &DatabaseController::rowCountChanged);
    }
}

QAbstractItemModel* DatabaseController::model() const
{
    return currentModel();
}

QSqlTableModel* DatabaseController::currentModel() const
{
    return m_currentTable == 0 ? m_snapshotsModel : m_entriesModel;
}

void DatabaseController::setCurrentTable(int index)
{
    if (index == m_currentTable || index < 0 || index > 1) return;
    m_currentTable = index;
    emit currentTableChanged();
    emit rowCountChanged();
}

int DatabaseController::rowCount() const
{
    return currentModel()->rowCount();
}

int DatabaseController::columnWidth(int column) const
{
    // Mirrors the column widths of the legacy Widgets DatabaseTab.
    if (m_currentTable == 0) {                       // snapshots: id, date
        switch (column) {
        case 0:  return 55;
        default: return 160;
        }
    }
    switch (column) {                                // asset_entries
    case 0:  return 55;    // id
    case 1:  return 100;   // snapshot_id
    case 2:  return 90;    // category
    case 3:  return 220;   // name
    default: return 140;   // value
    }
}

int DatabaseController::addRow()
{
    auto* m = currentModel();
    const int newRow = m->rowCount();
    m->insertRow(newRow);
    return newRow;
}

void DatabaseController::deleteRows(const QVariantList& rows)
{
    if (rows.isEmpty()) {
        setStatus(QStringLiteral("⚠  Select one or more rows to delete"), true);
        return;
    }

    // Unique row numbers, descending, so indices stay valid while removing.
    QList<int> sorted;
    for (const QVariant& v : rows) {
        const int r = v.toInt();
        if (!sorted.contains(r)) sorted.append(r);
    }
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());

    auto* m = currentModel();
    for (int r : sorted)
        m->removeRow(r);

    setStatus(QStringLiteral("Marked %1 row%2 for deletion — click Save to confirm")
                  .arg(sorted.size())
                  .arg(sorted.size() == 1 ? "" : "s"));
    emit rowCountChanged();
}

void DatabaseController::save()
{
    auto* m = currentModel();
    if (!m->submitAll()) {
        setStatus(QStringLiteral("⚠  ") + m->lastError().text(), true);
    } else {
        m->select();   // re-read so auto-assigned ids appear
        setStatus(QStringLiteral("✓  Saved"));
        emit changesSaved();
    }
}

void DatabaseController::revert()
{
    currentModel()->revertAll();
    currentModel()->select();
    setStatus(QStringLiteral("↩  Reverted"));
}

void DatabaseController::refresh()
{
    m_snapshotsModel->revertAll();
    m_snapshotsModel->select();
    m_entriesModel->revertAll();
    m_entriesModel->select();
    setStatus(QStringLiteral("✓  Refreshed"));
}

void DatabaseController::setStatus(const QString& msg, bool isError)
{
    m_statusText    = msg;
    m_statusIsError = isError;
    emit statusChanged();

    QTimer::singleShot(4000, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}

#include "DatabaseController.moc"
