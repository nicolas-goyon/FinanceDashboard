#include "EntryController.h"
#include "db/Database.h"
#include "utils/Formatter.h"

#include <QLocale>
#include <QTimer>

// ── CategoryModel ─────────────────────────────────────────────────────────────

CategoryModel::CategoryModel(QString assetType, QObject* parent)
    : QAbstractListModel(parent)
    , m_assetType(std::move(assetType))
{
}

int CategoryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant CategoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row& r = m_rows[index.row()];
    switch (role) {
    case NameRole:      return r.name;
    case ValueTextRole: return r.valueText;
    case DiffTextRole:  return diffText(r);
    case DiffStateRole: return diffState(r);
    default:            return {};
    }
}

QHash<int, QByteArray> CategoryModel::roleNames() const
{
    return {
        { NameRole,      "name"      },
        { ValueTextRole, "valueText" },
        { DiffTextRole,  "diffText"  },
        { DiffStateRole, "diffState" },
    };
}

double CategoryModel::total() const
{
    double t = 0.0;
    for (const Row& r : m_rows) {
        bool ok = false;
        const double v = QLocale::c().toDouble(r.valueText, &ok);
        if (ok) t += v;
    }
    return t;
}

QString CategoryModel::totalText() const
{
    return Fmt::money(total());
}

void CategoryModel::addRow()
{
    beginInsertRows({}, m_rows.size(), m_rows.size());
    m_rows.append(Row{});
    endInsertRows();
    emit totalChanged();
}

void CategoryModel::removeRow(int index)
{
    if (index < 0 || index >= m_rows.size()) return;
    beginRemoveRows({}, index, index);
    m_rows.remove(index);
    endRemoveRows();
    emit totalChanged();
}

void CategoryModel::setRowName(int index, const QString& name)
{
    if (index < 0 || index >= m_rows.size()) return;
    m_rows[index].name = name;
    // No dataChanged for NameRole — the edit came from the TextField itself.
}

void CategoryModel::setRowValue(int index, const QString& valueText)
{
    if (index < 0 || index >= m_rows.size()) return;
    m_rows[index].valueText = valueText;
    // Only refresh the diff badge — NOT ValueTextRole (would reset the cursor).
    const QModelIndex mi = this->index(index);
    emit dataChanged(mi, mi, { DiffTextRole, DiffStateRole });
    emit totalChanged();
}

void CategoryModel::fill(const QVector<Data::AssetEntry>& entries)
{
    beginResetModel();
    m_rows.clear();
    for (const Data::AssetEntry& e : entries) {
        Row r;
        r.name        = e.name;
        r.baseline    = e.value;
        r.hasBaseline = true;
        if (e.value != 0.0)
            r.valueText = QLocale::c().toString(e.value, 'f', 2);
        m_rows.append(r);
    }
    endResetModel();
    emit totalChanged();
}

void CategoryModel::clearRows()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit totalChanged();
}

QVector<Data::AssetEntry> CategoryModel::collect() const
{
    QVector<Data::AssetEntry> result;
    for (const Row& r : m_rows) {
        const QString name = r.name.trimmed();
        bool ok = false;
        const double val = QLocale::c().toDouble(r.valueText, &ok);
        if (!name.isEmpty() || ok)
            result.append({ name.isEmpty() ? QStringLiteral("(unnamed)") : name,
                            ok ? val : 0.0 });
    }
    return result;
}

// Diff badge logic — mirrors legacy EntryTab::refreshDiffBadge().
QString CategoryModel::diffText(const Row& r) const
{
    if (!r.hasBaseline || r.baseline == 0.0)
        return QStringLiteral("--.-%");

    bool ok = false;
    const double newVal = QLocale::c().toDouble(r.valueText, &ok);
    if (!ok || r.valueText.trimmed().isEmpty())
        return QStringLiteral("--.-%");

    const double pct = (newVal - r.baseline) / r.baseline * 100.0;
    if (pct > 0.0) return QStringLiteral("+%1%").arg(pct, 0, 'f', 1);
    if (pct < 0.0) return QStringLiteral("%1%").arg(pct, 0, 'f', 1);
    return QStringLiteral("0.0%");
}

QString CategoryModel::diffState(const Row& r) const
{
    if (!r.hasBaseline || r.baseline == 0.0) return QStringLiteral("none");

    bool ok = false;
    const double newVal = QLocale::c().toDouble(r.valueText, &ok);
    if (!ok || r.valueText.trimmed().isEmpty()) return QStringLiteral("none");

    const double pct = (newVal - r.baseline) / r.baseline * 100.0;
    if (pct > 0.0) return QStringLiteral("up");
    if (pct < 0.0) return QStringLiteral("down");
    return QStringLiteral("none");
}

// ── EntryController ───────────────────────────────────────────────────────────

EntryController::EntryController(QObject* parent)
    : QObject(parent)
    , m_stocks(new CategoryModel(QStringLiteral("Stock"), this))
    , m_coins (new CategoryModel(QStringLiteral("Coin"),  this))
    , m_metals(new CategoryModel(QStringLiteral("Metal"), this))
    , m_etfs  (new CategoryModel(QStringLiteral("ETF"),   this))
    , m_date(QDate::currentDate())
{
    for (CategoryModel* m : { m_stocks, m_coins, m_metals, m_etfs })
        connect(m, &CategoryModel::totalChanged,
                this, &EntryController::grandTotalChanged);

    // Pre-fill with the most recent snapshot (if any)
    const auto latest = Database::instance().loadLatestSnapshot();
    if (latest) {
        m_date = latest->date;
        m_stocks->fill(latest->stocks);
        m_coins ->fill(latest->coins);
        m_metals->fill(latest->metals);
        m_etfs  ->fill(latest->etfs);
    }
}

QString EntryController::grandTotalText() const
{
    return Fmt::money(m_stocks->total() + m_coins->total()
                      + m_metals->total() + m_etfs->total());
}

void EntryController::setDateYmd(int year, int month0, int day)
{
    const QDate d(year, month0 + 1, day);
    if (!d.isValid() || d > QDate::currentDate() || d == m_date)
        return;

    m_date = d;
    emit dateChanged();
    loadFor(d);
}

void EntryController::loadFor(const QDate& date)
{
    const auto snap = Database::instance().loadSnapshot(date);
    if (snap) {
        m_stocks->fill(snap->stocks);
        m_coins ->fill(snap->coins);
        m_metals->fill(snap->metals);
        m_etfs  ->fill(snap->etfs);
    } else {
        // No entry for this date — show a blank form.
        m_stocks->clearRows();
        m_coins ->clearRows();
        m_metals->clearRows();
        m_etfs  ->clearRows();
    }
}

void EntryController::save()
{
    Data::FinancialSnapshot snap;
    snap.date   = m_date;
    snap.stocks = m_stocks->collect();
    snap.coins  = m_coins ->collect();
    snap.metals = m_metals->collect();
    snap.etfs   = m_etfs  ->collect();

    if (Database::instance().saveSnapshot(snap)) {
        setStatus(QStringLiteral("✓  Saved"));
        emit snapshotSaved();
    } else {
        setStatus(QStringLiteral("⚠  Save failed — ")
                      + Database::instance().lastError(), /*isError=*/true);
    }
}

void EntryController::clearAll()
{
    m_stocks->clearRows();
    m_coins ->clearRows();
    m_metals->clearRows();
    m_etfs  ->clearRows();

    // Legacy behavior: resetting to today also reloads today's snapshot if
    // one exists (dateChanged → load). Same date = rows stay cleared.
    if (m_date != QDate::currentDate()) {
        m_date = QDate::currentDate();
        emit dateChanged();
        loadFor(m_date);
    }
}

void EntryController::setStatus(const QString& msg, bool isError)
{
    m_statusText    = msg;
    m_statusIsError = isError;
    emit statusChanged();

    QTimer::singleShot(3000, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}
