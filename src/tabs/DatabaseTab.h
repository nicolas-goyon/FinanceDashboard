#pragma once

#include <QWidget>

class QTableView;
class QSqlTableModel;
class QLabel;
class QStackedWidget;
class QComboBox;
class QPushButton;

// ── DatabaseTab ───────────────────────────────────────────────────────────────
//
// Raw, table-level view of the SQLite database.
// Shows the `snapshots` and `asset_entries` tables in editable QTableViews.
//
// Editing model: OnManualSubmit — changes are buffered until the user clicks
// "Save".  "Revert" discards all pending changes.
//
// The `id` column is display-only (auto-assigned by SQLite on save).

class DatabaseTab : public QWidget
{
    Q_OBJECT

public:
    explicit DatabaseTab(QWidget* parent = nullptr);

private slots:
    void onTableSelected(int index);
    void onAddRow();
    void onDeleteRow();
    void onSave();
    void onRevert();
    void onRefresh();
    void updateRowCount();

private:
    void buildUi();
    void setupModels();
    void applyViewStyle(QTableView* view);

    QTableView*     currentView()  const;
    QSqlTableModel* currentModel() const;
    void setStatus(const QString& msg, bool isError = false);

    // ── Widgets ───────────────────────────────────────────────────────────────
    QComboBox*       m_tableSelector{nullptr};
    QStackedWidget*  m_stack{nullptr};
    QTableView*      m_snapshotsView{nullptr};
    QTableView*      m_entriesView{nullptr};
    QSqlTableModel*  m_snapshotsModel{nullptr};
    QSqlTableModel*  m_entriesModel{nullptr};
    QLabel*          m_statusLabel{nullptr};
    QLabel*          m_rowCountLabel{nullptr};
    QPushButton*     m_saveBtn{nullptr};
    QPushButton*     m_revertBtn{nullptr};
};
