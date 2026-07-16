#pragma once

#include "models/AssetEntry.h"

#include <optional>
#include <QWidget>
#include <QVector>

class QLineEdit;
class QDateEdit;
class QLabel;
class QVBoxLayout;

// ── Internal row / block helpers ──────────────────────────────────────────────

struct AssetRow {
    QWidget*   container{nullptr};
    QLineEdit* nameEdit{nullptr};
    QLineEdit* valueEdit{nullptr};
    QLabel*    diffBadge{nullptr};
    double     baselineValue{0.0};
    bool       hasBaseline{false};
};

struct CategoryBlock {
    QString           assetType;          ///< "Stock", "Coin", "Metal", "ETF"
    QVBoxLayout*      rowLayout{nullptr};
    QVector<AssetRow> rows;
    QLabel*           totalLabel{nullptr};
};

// ── EntryTab ──────────────────────────────────────────────────────────────────

class EntryTab : public QWidget
{
    Q_OBJECT

public:
    explicit EntryTab(QWidget* parent = nullptr);

private slots:
    void onSave();
    void onClear();
    void onDateChanged(const QDate& date);
    void updateTotals();

private:
    void buildUi();

    // Category section builder — returns the outer QFrame.
    QWidget* buildCategorySection(const QString& title,
                                   const QString& assetType,
                                   CategoryBlock& block);

    // Row management
    void addRow(CategoryBlock& block,
                const std::optional<Data::AssetEntry>& prefill = std::nullopt);
    void removeRow(CategoryBlock& block, QWidget* rowWidget);
    void clearBlock(CategoryBlock& block);
    void clearAllRows();

    // Snapshot ↔ form
    void fillFromSnapshot(const Data::FinancialSnapshot& snap);
    QVector<Data::AssetEntry> collectEntries(const CategoryBlock& block) const;

    // Inline status banner (auto-clears after 3 s)
    void setStatus(const QString& msg, bool isError = false);

    // Update the diff badge for a single row based on the current typed value.
    static void refreshDiffBadge(QLabel* badge,
                                  bool    hasBaseline,
                                  double  baseline,
                                  const QString& currentText);

    // ── Persistent widgets ────────────────────────────────────────────────────
    QDateEdit* m_dateEdit{nullptr};
    QLabel*    m_grandTotalLabel{nullptr};
    QLabel*    m_statusLabel{nullptr};

    CategoryBlock m_stocks;
    CategoryBlock m_coins;
    CategoryBlock m_metals;
    CategoryBlock m_etfs;

    // Guard: prevents onDateChanged from firing during programmatic date updates
    bool m_loadingData{false};
};
