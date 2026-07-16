#include "PortfolioTab.h"
#include "data/DebugData.h"
#include "utils/Formatter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QFrame>

static QTableWidgetItem* cell(const QString& text, Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

PortfolioTab::PortfolioTab(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void PortfolioTab::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // Header
    auto* title = new QLabel("Holdings");
    title->setStyleSheet("font-size: 20px; font-weight: 700; color: #cdd6f4;");
    root->addWidget(title);

    auto* badge = new QLabel("[DEBUG DATA — not live]");
    badge->setStyleSheet("color: #f38ba8; font-size: 11px;");
    root->addWidget(badge);

    // Table
    const QStringList headers = {
        "Ticker", "Name", "Shares", "Avg Cost", "Price", "Market Value", "P&L", "P&L %"
    };

    m_table = new QTableWidget(0, headers.size(), this);
    m_table->setHorizontalHeaderLabels(headers);

    // Ticker and Name headers align left; all others default to right
    m_table->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->horizontalHeader()->setStyleSheet(
        "QHeaderView::section { background: #181825; color: #6c7086; "
        "font-weight: 600; font-size: 11px; border: none; padding: 6px; }"
    );
    m_table->setStyleSheet(
        "QTableWidget { background: #11111b; color: #cdd6f4; "
        "gridline-color: #313244; border: none; font-size: 13px; }"
        "QTableWidget::item { padding: 6px; }"
        "QTableWidget::item:selected { background: #313244; }"
    );
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet(m_table->styleSheet() +
        "QTableWidget { alternate-background-color: #1e1e2e; }");

    populateTable();
    root->addWidget(m_table);

    // Footer summary
    auto* footer = new QLabel();
    const auto holdings = Debug::sampleHoldings();
    double totalMV = 0;
    for (const auto& h : holdings)
        totalMV += h.shares * h.currentPrice;
    footer->setText(QString("Total Market Value:  %1   |   Positions: %2")
                    .arg(Fmt::money(totalMV))
                    .arg(holdings.size()));
    footer->setStyleSheet("color: #888; font-size: 12px; padding: 4px 0;");
    root->addWidget(footer);
}

void PortfolioTab::populateTable()
{
    const auto holdings = Debug::sampleHoldings();
    m_table->setRowCount(holdings.size());

    for (int row = 0; row < holdings.size(); ++row) {
        const auto& h = holdings[row];

        double marketValue = h.shares * h.currentPrice;
        double cost        = h.shares * h.avgCost;
        double pnl         = marketValue - cost;
        double pnlPct      = (pnl / cost) * 100.0;
        bool   positive    = pnl >= 0;
        QString pnlColor   = positive ? "#a6e3a1" : "#f38ba8";

        m_table->setItem(row, 0, cell(h.ticker,  Qt::AlignLeft | Qt::AlignVCenter));
        m_table->setItem(row, 1, cell(h.name,    Qt::AlignLeft | Qt::AlignVCenter));
        m_table->setItem(row, 2, cell(QString::number(h.shares, 'f', 2)));
        m_table->setItem(row, 3, cell(Fmt::money(h.avgCost)));
        m_table->setItem(row, 4, cell(Fmt::money(h.currentPrice)));
        m_table->setItem(row, 5, cell(Fmt::money(marketValue)));

        auto* pnlItem = cell(Fmt::money(pnl, /*showPlus=*/true));
        pnlItem->setForeground(QColor(pnlColor));
        m_table->setItem(row, 6, pnlItem);

        auto* pctItem = cell(QString("%1%2%").arg(positive ? "+" : "").arg(std::abs(pnlPct), 0, 'f', 2));
        pctItem->setForeground(QColor(pnlColor));
        m_table->setItem(row, 7, pctItem);
    }
}
