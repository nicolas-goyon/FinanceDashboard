#include "DatabaseTab.h"
#include "db/Database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QTableView>
#include <QStackedWidget>
#include <QPushButton>
#include <QHeaderView>
#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QSqlError>
#include <QFrame>
#include <QTimer>
#include <QItemSelectionModel>

// ── Style constants ───────────────────────────────────────────────────────────

namespace {

const char* C_TEXT    = "#cdd6f4";
const char* C_MUTED   = "#6c7086";
const char* C_SURFACE = "#1e1e2e";
const char* C_MANTLE  = "#181825";
const char* C_BORDER  = "#313244";
const char* C_GREEN   = "#a6e3a1";
const char* C_RED     = "#f38ba8";
const char* C_BLUE    = "#89b4fa";

QFrame* hline()
{
    auto* f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setStyleSheet(QString("color: %1;").arg(C_BORDER));
    return f;
}

QString btnStyle(const char* bg = "#1e1e2e",
                 const char* hoverBg = "#313244",
                 const char* border = "#313244")
{
    return QString(
        "QPushButton {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 4px;"
        "  padding: 6px 16px; font-size: 12px;"
        "}"
        "QPushButton:hover   { background: %4; }"
        "QPushButton:pressed { background: #45475a; }"
        "QPushButton:disabled { color: #45475a; border-color: #45475a; }")
        .arg(bg).arg(C_TEXT).arg(border).arg(hoverBg);
}

} // namespace

// ── Private model: id column is read-only ─────────────────────────────────────

class DbTableModel : public QSqlTableModel
{
    Q_OBJECT
public:
    using QSqlTableModel::QSqlTableModel;

    Qt::ItemFlags flags(const QModelIndex& idx) const override
    {
        const auto base = QSqlTableModel::flags(idx);
        // Column 0 is always the integer primary key — lock it.
        if (idx.column() == 0)
            return base & ~Qt::ItemIsEditable;
        return base;
    }
};

// ── DatabaseTab ───────────────────────────────────────────────────────────────

DatabaseTab::DatabaseTab(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    setupModels();
}

// ── UI construction ───────────────────────────────────────────────────────────

void DatabaseTab::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // ── Header ────────────────────────────────────────────────────────────────
    {
        auto* hdr = new QHBoxLayout;

        auto* title = new QLabel("Database");
        title->setStyleSheet("font-size: 20px; font-weight: 700; color: #cdd6f4;");
        hdr->addWidget(title);

        hdr->addStretch();

        auto* refreshBtn = new QPushButton("↻  Refresh");
        refreshBtn->setStyleSheet(btnStyle());
        refreshBtn->setToolTip("Re-read all data from the database (discards unsaved changes)");
        hdr->addWidget(refreshBtn);
        connect(refreshBtn, &QPushButton::clicked, this, &DatabaseTab::onRefresh);

        root->addLayout(hdr);
    }

    // ── Table selector ────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);

        auto* lbl = new QLabel("Table:");
        lbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(C_MUTED));
        row->addWidget(lbl);

        m_tableSelector = new QComboBox;
        m_tableSelector->addItem("snapshots");
        m_tableSelector->addItem("asset_entries");
        m_tableSelector->setFixedWidth(180);
        m_tableSelector->setStyleSheet(
            QString("QComboBox {"
                    "  background: %1; color: %2;"
                    "  border: 1px solid %3; border-radius: 4px;"
                    "  padding: 4px 8px; font-size: 12px;"
                    "}"
                    "QComboBox::drop-down { border: none; width: 18px; }"
                    "QComboBox QAbstractItemView {"
                    "  background: %1; color: %2;"
                    "  selection-background-color: %3;"
                    "}")
                .arg(C_SURFACE).arg(C_TEXT).arg(C_BORDER)
        );
        row->addWidget(m_tableSelector);

        m_rowCountLabel = new QLabel;
        m_rowCountLabel->setStyleSheet(
            QString("color: %1; font-size: 11px;").arg(C_MUTED)
        );
        row->addWidget(m_rowCountLabel);

        row->addStretch();
        root->addLayout(row);
    }

    root->addWidget(hline());

    // ── Table views in a stacked widget ──────────────────────────────────────
    m_stack = new QStackedWidget;

    m_snapshotsView = new QTableView;
    applyViewStyle(m_snapshotsView);
    m_stack->addWidget(m_snapshotsView);

    m_entriesView = new QTableView;
    applyViewStyle(m_entriesView);
    m_stack->addWidget(m_entriesView);

    root->addWidget(m_stack, /*stretch=*/1);

    root->addWidget(hline());

    // ── Bottom action bar ─────────────────────────────────────────────────────
    {
        auto* bar = new QHBoxLayout;
        bar->setSpacing(8);

        auto* addBtn = new QPushButton("＋  Add Row");
        addBtn->setStyleSheet(btnStyle());
        bar->addWidget(addBtn);

        auto* delBtn = new QPushButton("✕  Delete Selected");
        delBtn->setStyleSheet(
            QString("QPushButton {"
                    "  background: transparent; color: %1;"
                    "  border: 1px solid %2; border-radius: 4px;"
                    "  padding: 6px 16px; font-size: 12px;"
                    "}"
                    "QPushButton:hover   { background: #3d2028; color: %1; border-color: %1; }"
                    "QPushButton:pressed { background: #2d1820; }"
                    "QPushButton:disabled { color: #45475a; border-color: #45475a; }")
                .arg(C_RED).arg(C_BORDER)
        );
        bar->addWidget(delBtn);

        bar->addStretch();

        m_statusLabel = new QLabel;
        m_statusLabel->setStyleSheet(
            QString("font-size: 11px; padding: 0 4px; color: %1;").arg(C_GREEN)
        );
        bar->addWidget(m_statusLabel);

        m_revertBtn = new QPushButton("Revert");
        m_revertBtn->setStyleSheet(btnStyle());
        m_revertBtn->setToolTip("Discard all unsaved changes");
        bar->addWidget(m_revertBtn);

        m_saveBtn = new QPushButton("Save Changes");
        m_saveBtn->setStyleSheet(
            QString("QPushButton {"
                    "  background: #1e3a5f; color: %1;"
                    "  border: 1px solid %2; border-radius: 4px;"
                    "  padding: 6px 24px; font-size: 12px; font-weight: 600;"
                    "}"
                    "QPushButton:hover   { background: #264d80; }"
                    "QPushButton:pressed { background: #1a3050; }"
                    "QPushButton:disabled { color: #45475a; border-color: #45475a; background: #181825; }")
                .arg(C_TEXT).arg(C_BLUE)
        );
        bar->addWidget(m_saveBtn);

        root->addLayout(bar);

        connect(addBtn,          &QPushButton::clicked, this, &DatabaseTab::onAddRow);
        connect(delBtn,          &QPushButton::clicked, this, &DatabaseTab::onDeleteRow);
        connect(m_saveBtn,       &QPushButton::clicked, this, &DatabaseTab::onSave);
        connect(m_revertBtn,     &QPushButton::clicked, this, &DatabaseTab::onRevert);
        connect(m_tableSelector, qOverload<int>(&QComboBox::currentIndexChanged),
                this, &DatabaseTab::onTableSelected);
    }
}

void DatabaseTab::applyViewStyle(QTableView* view)
{
    view->setStyleSheet(
        QString("QTableView {"
                "  background: %1; color: %2;"
                "  gridline-color: %3;"
                "  border: 1px solid %3; border-radius: 6px;"
                "  selection-background-color: #2a2a40;"
                "  selection-color: %2;"
                "}"
                "QTableView::item { padding: 4px 8px; }"
                "QTableView::item:selected {"
                "  background: #2a2a40; color: %2;"
                "}"
                "QHeaderView::section {"
                "  background: %4; color: %5;"
                "  border: none; border-right: 1px solid %3;"
                "  border-bottom: 1px solid %3;"
                "  padding: 6px 8px; font-weight: 600; font-size: 12px;"
                "}"
                "QHeaderView::section:horizontal:last { border-right: none; }")
            .arg(C_SURFACE).arg(C_TEXT).arg(C_BORDER).arg(C_MANTLE).arg(C_MUTED)
    );

    view->horizontalHeader()->setStretchLastSection(true);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->verticalHeader()->setDefaultSectionSize(30);
    view->verticalHeader()->hide();
    view->setAlternatingRowColors(false);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setSortingEnabled(true);
    view->setEditTriggers(QAbstractItemView::DoubleClicked |
                          QAbstractItemView::EditKeyPressed);
}

// ── Model setup ───────────────────────────────────────────────────────────────

void DatabaseTab::setupModels()
{
    QSqlDatabase db = Database::instance().sqlDatabase();

    // ── Snapshots ─────────────────────────────────────────────────────────────
    m_snapshotsModel = new DbTableModel(this, db);
    m_snapshotsModel->setTable("snapshots");
    m_snapshotsModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_snapshotsModel->setHeaderData(0, Qt::Horizontal, "id");
    m_snapshotsModel->setHeaderData(1, Qt::Horizontal, "date");
    m_snapshotsModel->select();

    m_snapshotsView->setModel(m_snapshotsModel);
    m_snapshotsView->setColumnWidth(0, 55);
    m_snapshotsView->setColumnWidth(1, 120);

    // ── Asset entries ─────────────────────────────────────────────────────────
    m_entriesModel = new DbTableModel(this, db);
    m_entriesModel->setTable("asset_entries");
    m_entriesModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_entriesModel->setHeaderData(0, Qt::Horizontal, "id");
    m_entriesModel->setHeaderData(1, Qt::Horizontal, "snapshot_id");
    m_entriesModel->setHeaderData(2, Qt::Horizontal, "category");
    m_entriesModel->setHeaderData(3, Qt::Horizontal, "name");
    m_entriesModel->setHeaderData(4, Qt::Horizontal, "value");
    m_entriesModel->select();

    m_entriesView->setModel(m_entriesModel);
    m_entriesView->setColumnWidth(0, 55);
    m_entriesView->setColumnWidth(1, 100);
    m_entriesView->setColumnWidth(2, 90);
    m_entriesView->setColumnWidth(3, 200);
    m_entriesView->setColumnWidth(4, 120);

    // Start on the snapshots table
    m_stack->setCurrentIndex(0);
    updateRowCount();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QTableView* DatabaseTab::currentView() const
{
    return m_tableSelector->currentIndex() == 0 ? m_snapshotsView : m_entriesView;
}

QSqlTableModel* DatabaseTab::currentModel() const
{
    return m_tableSelector->currentIndex() == 0 ? m_snapshotsModel : m_entriesModel;
}

void DatabaseTab::setStatus(const QString& msg, bool isError)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("font-size: 11px; padding: 0 4px; color: %1;")
            .arg(isError ? C_RED : C_GREEN)
    );
    QTimer::singleShot(4000, this, [this]() { m_statusLabel->clear(); });
}

void DatabaseTab::updateRowCount()
{
    const int rows = currentModel()->rowCount();
    m_rowCountLabel->setText(QString("%1 row%2").arg(rows).arg(rows == 1 ? "" : "s"));
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void DatabaseTab::onTableSelected(int index)
{
    m_stack->setCurrentIndex(index);
    updateRowCount();
}

void DatabaseTab::onAddRow()
{
    auto* model = currentModel();
    auto* view  = currentView();

    const int newRow = model->rowCount();
    model->insertRow(newRow);

    // Scroll to and select the new row, start editing first editable column.
    const QModelIndex first = model->index(newRow, 1); // column 1 — skip id
    view->scrollTo(first);
    view->selectionModel()->setCurrentIndex(first, QItemSelectionModel::ClearAndSelect |
                                                        QItemSelectionModel::Rows);
    view->edit(first);
    updateRowCount();
}

void DatabaseTab::onDeleteRow()
{
    auto* model = currentModel();
    auto* view  = currentView();

    const auto selection = view->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        setStatus("⚠  Select one or more rows to delete", /*isError=*/true);
        return;
    }

    // Remove in reverse order so row indices stay valid.
    QList<int> rows;
    for (const auto& idx : selection)
        rows.append(idx.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    for (int r : rows)
        model->removeRow(r);

    setStatus(QString("Marked %1 row%2 for deletion — click Save to confirm")
                  .arg(rows.size()).arg(rows.size() == 1 ? "" : "s"));
    updateRowCount();
}

void DatabaseTab::onSave()
{
    auto* model = currentModel();
    if (!model->submitAll()) {
        setStatus("⚠  " + model->lastError().text(), /*isError=*/true);
    } else {
        model->select(); // re-read so auto-assigned ids appear
        updateRowCount();
        setStatus("✓  Saved");
    }
}

void DatabaseTab::onRevert()
{
    currentModel()->revertAll();
    currentModel()->select();
    updateRowCount();
    setStatus("↩  Reverted");
}

void DatabaseTab::onRefresh()
{
    // Revert any pending changes on both models and re-read from DB.
    m_snapshotsModel->revertAll();
    m_snapshotsModel->select();
    m_entriesModel->revertAll();
    m_entriesModel->select();
    updateRowCount();
    setStatus("✓  Refreshed");
}

// Required so Q_OBJECT in DbTableModel (defined in .cpp) is processed by moc.
#include "DatabaseTab.moc"
