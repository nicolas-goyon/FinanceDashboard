#include "ImportTab.h"
#include "utils/FileFinder.h"
#include "utils/JsonReader.h"
#include "utils/Formatter.h"
#include "models/PortfolioData.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>

// ── Style constants ───────────────────────────────────────────────────────────

namespace {

constexpr const char* COLOR_TEXT    = "#cdd6f4";
constexpr const char* COLOR_MUTED   = "#6c7086";
constexpr const char* COLOR_SURFACE = "#1e1e2e";
constexpr const char* COLOR_BORDER  = "#313244";
constexpr const char* COLOR_GREEN   = "#a6e3a1";
constexpr const char* COLOR_RED     = "#f38ba8";
constexpr const char* COLOR_BLUE    = "#89b4fa";

// Section heading label
QLabel* sectionHeading(const QString& text)
{
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: 700;"
                "border-bottom: 1px solid %2; padding-bottom: 4px;")
            .arg(COLOR_BLUE).arg(COLOR_BORDER)
    );
    return lbl;
}

// A key / value row added to a QGridLayout
void addRow(QGridLayout* grid, int row,
            const QString& key, const QString& value,
            const QString& valueColor = COLOR_TEXT)
{
    auto* k = new QLabel(key);
    k->setStyleSheet(QString("color: %1; font-size: 12px;").arg(COLOR_MUTED));

    auto* v = new QLabel(value);
    v->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: 600;").arg(valueColor)
    );
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);

    grid->addWidget(k, row, 0);
    grid->addWidget(v, row, 1);
}

// Horizontal separator line
QFrame* separator()
{
    auto* f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Sunken);
    f->setStyleSheet(QString("color: %1;").arg(COLOR_BORDER));
    return f;
}

} // anonymous namespace

// ── ImportTab ─────────────────────────────────────────────────────────────────

ImportTab::ImportTab(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ImportTab::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* title = new QLabel("Import Data");
    title->setStyleSheet("font-size: 20px; font-weight: 700; color: #cdd6f4;");
    root->addWidget(title);

    auto* subtitle = new QLabel("Load a portfolio JSON file to preview its contents.");
    subtitle->setStyleSheet(QString("color: %1; font-size: 12px;").arg(COLOR_MUTED));
    root->addWidget(subtitle);

    root->addWidget(separator());

    // ── File picker row ───────────────────────────────────────────────────────
    auto* pickerRow = new QHBoxLayout;
    pickerRow->setSpacing(8);

    auto* pathLabel = new QLabel("File path:");
    pathLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(COLOR_MUTED));
    pathLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    m_pathEdit = new QLineEdit;
    m_pathEdit->setPlaceholderText("C:/data/portfolio.json");
    m_pathEdit->setStyleSheet(
        QString("QLineEdit {"
                "  background: %1; color: %2;"
                "  border: 1px solid %3; border-radius: 4px;"
                "  padding: 5px 8px; font-size: 12px;"
                "}"
                "QLineEdit:focus { border-color: %4; }")
            .arg(COLOR_SURFACE).arg(COLOR_TEXT).arg(COLOR_BORDER).arg(COLOR_BLUE)
    );

    auto* browseBtn = new QPushButton("Browse…");
    browseBtn->setStyleSheet(
        QString("QPushButton {"
                "  background: %1; color: %2;"
                "  border: 1px solid %3; border-radius: 4px;"
                "  padding: 5px 14px; font-size: 12px;"
                "}"
                "QPushButton:hover  { background: #313244; }"
                "QPushButton:pressed{ background: #45475a; }")
            .arg(COLOR_SURFACE).arg(COLOR_TEXT).arg(COLOR_BORDER)
    );

    m_importBtn = new QPushButton("Import");
    m_importBtn->setStyleSheet(
        QString("QPushButton {"
                "  background: #1e3a5f; color: %1;"
                "  border: 1px solid %2; border-radius: 4px;"
                "  padding: 5px 18px; font-size: 12px; font-weight: 600;"
                "}"
                "QPushButton:hover  { background: #264d80; }"
                "QPushButton:pressed{ background: #1a3050; }"
                "QPushButton:disabled { background: #1e1e2e; color: #45475a; }")
            .arg(COLOR_TEXT).arg(COLOR_BLUE)
    );

    pickerRow->addWidget(pathLabel);
    pickerRow->addWidget(m_pathEdit, /*stretch=*/1);
    pickerRow->addWidget(browseBtn);
    pickerRow->addWidget(m_importBtn);
    root->addLayout(pickerRow);

    // ── Status label ──────────────────────────────────────────────────────────
    m_statusLabel = new QLabel("No file imported yet.");
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; padding: 2px 0;").arg(COLOR_MUTED)
    );
    root->addWidget(m_statusLabel);

    root->addWidget(separator());

    // ── Scroll area (results) ─────────────────────────────────────────────────
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet("QScrollArea { background: transparent; }");

    showMessage("Import a JSON file to see results here.");

    root->addWidget(m_scroll, /*stretch=*/1);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(browseBtn,   &QPushButton::clicked, this, &ImportTab::onBrowse);
    connect(m_importBtn, &QPushButton::clicked, this, &ImportTab::onImport);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ImportTab::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open Portfolio JSON",
        m_pathEdit->text().isEmpty() ? QDir::homePath() : m_pathEdit->text(),
        "JSON files (*.json);;All files (*)"
    );
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}

void ImportTab::onImport()
{
    const QString path = m_pathEdit->text().trimmed();

    if (path.isEmpty()) {
        showMessage("Please enter or browse to a file path.", /*isError=*/true);
        m_statusLabel->setText("⚠  No path specified.");
        return;
    }

    if (!FileFinder::exists(path)) {
        showMessage(QString("File not found:\n%1").arg(path), /*isError=*/true);
        m_statusLabel->setText("⚠  File not found.");
        return;
    }

    m_importBtn->setEnabled(false);
    m_statusLabel->setText("Reading…");

    const auto result = JsonReader::read<Data::PortfolioSummary>(path);

    m_importBtn->setEnabled(true);

    if (!result) {
        showMessage("Failed to parse the file.\n"
                    "Make sure it is a valid portfolio JSON.", /*isError=*/true);
        m_statusLabel->setText("⚠  Parse error — check the application log for details.");
        return;
    }

    const QFileInfo fi(path);
    m_statusLabel->setText(
        QString("✓  Imported: %1   (%2 data points)")
            .arg(fi.fileName())
            .arg(result->data.size())
    );
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; padding: 2px 0;").arg(COLOR_GREEN)
    );

    showResults(*result);
}

// ── Results rendering ─────────────────────────────────────────────────────────

QWidget* ImportTab::resetResultsWidget()
{
    delete m_resultsWidget;
    m_resultsWidget = new QWidget;
    m_resultsWidget->setStyleSheet("background: transparent;");
    m_scroll->setWidget(m_resultsWidget);
    return m_resultsWidget;
}

void ImportTab::showMessage(const QString& msg, bool isError)
{
    QWidget* container = resetResultsWidget();
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(0, 20, 0, 0);

    auto* lbl = new QLabel(msg);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(
        QString("color: %1; font-size: 13px;").arg(isError ? COLOR_RED : COLOR_MUTED)
    );
    lbl->setWordWrap(true);

    lay->addWidget(lbl);
    lay->addStretch();
}

void ImportTab::showResults(const Data::PortfolioSummary& s)
{
    QWidget* container = resetResultsWidget();
    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 4, 16, 16);
    root->setSpacing(20);

    // ── Summary card ──────────────────────────────────────────────────────────
    root->addWidget(sectionHeading("Portfolio Summary"));

    auto* summaryCard = new QFrame;
    summaryCard->setStyleSheet(
        QString("QFrame { background: %1; border-radius: 8px; padding: 2px; }").arg(COLOR_SURFACE)
    );
    auto* grid = new QGridLayout(summaryCard);
    grid->setContentsMargins(14, 10, 14, 10);
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);

    const bool perfPositive = s.performance >= 0;
    const bool gainsPositive = s.gains >= 0;

    int r = 0;
    addRow(grid, r++, "Timeframe",         s.timeframe.isEmpty() ? "—" : s.timeframe);
    addRow(grid, r++, "Portfolio Value",   Fmt::money(s.overallPortfolioAmount));
    addRow(grid, r++, "Performance",
           (perfPositive ? "+" : "") + QString::number(s.performance, 'f', 2) + " %",
           perfPositive ? COLOR_GREEN : COLOR_RED);
    addRow(grid, r++, "Gains",             Fmt::money(s.gains, /*showPlus=*/true),
           gainsPositive ? COLOR_GREEN : COLOR_RED);
    addRow(grid, r++, "Dividends",         Fmt::money(s.dividends));
    addRow(grid, r++, "Buy",
           QString("%1  (%2 trade%3)")
               .arg(Fmt::money(s.buyAmount))
               .arg(s.buyCount)
               .arg(s.buyCount == 1 ? "" : "s"));
    addRow(grid, r++, "Sell",
           QString("%1  (%2 trade%3)")
               .arg(Fmt::money(s.sellAmount))
               .arg(s.sellCount)
               .arg(s.sellCount == 1 ? "" : "s"));
    addRow(grid, r++, "Best Asset",
           s.bestAsset.isEmpty() ? "—" : s.bestAsset);

    root->addWidget(summaryCard);

    // ── Time-series preview ───────────────────────────────────────────────────
    const int total = s.data.size();

    root->addWidget(sectionHeading(
        QString("Time Series  (%1 point%2)").arg(total).arg(total == 1 ? "" : "s")
    ));

    if (total == 0) {
        auto* empty = new QLabel("No data points in this file.");
        empty->setStyleSheet(QString("color: %1; font-size: 12px;").arg(COLOR_MUTED));
        root->addWidget(empty);
    } else {
        // Show at most 200 rows in the preview table
        constexpr int MAX_PREVIEW = 200;
        const int rowCount = qMin(total, MAX_PREVIEW);

        auto* table = new QTableWidget(rowCount, 2);
        table->setHorizontalHeaderLabels({"Timestamp", "Value"});
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->horizontalHeader()->setStyleSheet(
            QString("QHeaderView::section { background: #181825; color: %1; "
                    "font-weight: 600; font-size: 11px; border: none; padding: 6px; }")
                .arg(COLOR_MUTED)
        );
        table->setStyleSheet(
            QString("QTableWidget { background: %1; color: %2; "
                    "gridline-color: %3; border: none; font-size: 12px; }"
                    "QTableWidget::item { padding: 5px; }"
                    "QTableWidget::item:selected { background: %3; }"
                    "QTableWidget { alternate-background-color: #181825; }")
                .arg(COLOR_SURFACE).arg(COLOR_TEXT).arg(COLOR_BORDER)
        );
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setAlternatingRowColors(true);

        auto makeItem = [](const QString& text, Qt::Alignment align) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            return item;
        };

        for (int i = 0; i < rowCount; ++i) {
            const auto& dp = s.data[i];
            table->setItem(i, 0, makeItem(dp.time, Qt::AlignLeft | Qt::AlignVCenter));
            table->setItem(i, 1, makeItem(Fmt::money(dp.value), Qt::AlignRight | Qt::AlignVCenter));
        }

        // Cap height so the table doesn't swallow the whole scroll area
        table->setMinimumHeight(160);
        table->setMaximumHeight(400);
        root->addWidget(table);

        if (total > MAX_PREVIEW) {
            auto* note = new QLabel(
                QString("… and %1 more points (preview capped at %2).")
                    .arg(total - MAX_PREVIEW).arg(MAX_PREVIEW)
            );
            note->setStyleSheet(QString("color: %1; font-size: 11px;").arg(COLOR_MUTED));
            root->addWidget(note);
        }
    }

    root->addStretch();
}
