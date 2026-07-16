#include "EntryTab.h"
#include "db/Database.h"
#include "utils/Formatter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDateEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QDoubleValidator>
#include <QTimer>

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

QString fieldStyle(const char* focusColor = "#89b4fa")
{
    return QString(
        "QLineEdit {"
        "  background: #11111b; color: %1;"
        "  border: 1px solid %2; border-radius: 4px;"
        "  padding: 4px 8px; font-size: 12px;"
        "}"
        "QLineEdit:focus { border-color: %3; }"
    ).arg(C_TEXT).arg(C_BORDER).arg(focusColor);
}

} // namespace

// ── EntryTab ──────────────────────────────────────────────────────────────────

EntryTab::EntryTab(QWidget* parent)
    : QWidget(parent)
{
    m_stocks.assetType = "Stock";
    m_coins .assetType = "Coin";
    m_metals.assetType = "Metal";
    m_etfs  .assetType = "ETF";

    buildUi();

    // Pre-fill with the most recent snapshot (if any)
    const auto latest = Database::instance().loadLatestSnapshot();
    if (latest) {
        m_loadingData = true;
        m_dateEdit->setDate(latest->date);
        m_loadingData = false;
        fillFromSnapshot(*latest);
    }
}

// ── UI construction ───────────────────────────────────────────────────────────

void EntryTab::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* title = new QLabel("New Entry");
    title->setStyleSheet("font-size: 20px; font-weight: 700; color: #cdd6f4;");
    root->addWidget(title);

    auto* subtitle = new QLabel("Record current values for each asset category.");
    subtitle->setStyleSheet(QString("color: %1; font-size: 12px;").arg(C_MUTED));
    root->addWidget(subtitle);

    root->addWidget(hline());

    // ── Date row ──────────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);

        auto* lbl = new QLabel("Date:");
        lbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(C_MUTED));
        lbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

        m_dateEdit = new QDateEdit(QDate::currentDate());
        m_dateEdit->setDisplayFormat("dd/MM/yyyy");
        m_dateEdit->setCalendarPopup(true);
        m_dateEdit->setMaximumDate(QDate::currentDate());
        m_dateEdit->setMaximumWidth(160);
        m_dateEdit->setStyleSheet(
            QString("QDateEdit {"
                    "  background: %1; color: %2;"
                    "  border: 1px solid %3; border-radius: 4px;"
                    "  padding: 4px 8px; font-size: 12px;"
                    "}"
                    "QDateEdit::drop-down { border: none; width: 18px; }"
                    "QDateEdit:focus { border-color: %4; }")
                .arg(C_SURFACE).arg(C_TEXT).arg(C_BORDER).arg(C_BLUE)
        );

        auto* hint = new QLabel("defaults to today  —  older dates allowed");
        hint->setStyleSheet(QString("color: %1; font-size: 11px;").arg(C_MUTED));

        row->addWidget(lbl);
        row->addWidget(m_dateEdit);
        row->addWidget(hint);
        row->addStretch();
        root->addLayout(row);
    }

    root->addWidget(hline());

    // ── Category sections in a scroll area ───────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; }");

    auto* scrollContent = new QWidget;
    scrollContent->setStyleSheet("background: transparent;");
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 12, 8);
    scrollLayout->setSpacing(14);

    scrollLayout->addWidget(buildCategorySection("Stocks", "Stock",  m_stocks));
    scrollLayout->addWidget(buildCategorySection("Coins",  "Coin",   m_coins));
    scrollLayout->addWidget(buildCategorySection("Metals", "Metal",  m_metals));
    scrollLayout->addWidget(buildCategorySection("ETFs",   "ETF",    m_etfs));
    scrollLayout->addStretch();

    scroll->setWidget(scrollContent);
    root->addWidget(scroll, /*stretch=*/1);

    root->addWidget(hline());

    // ── Footer ────────────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);

        m_grandTotalLabel = new QLabel(
            QString("Grand Total:  %1").arg(Fmt::money(0.0))
        );
        m_grandTotalLabel->setStyleSheet(
            QString("color: %1; font-size: 14px; font-weight: 700;").arg(C_TEXT)
        );

        m_statusLabel = new QLabel;
        m_statusLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; padding: 0 4px;").arg(C_GREEN)
        );

        auto* clearBtn = new QPushButton("Clear all");
        clearBtn->setStyleSheet(
            QString("QPushButton {"
                    "  background: %1; color: %2;"
                    "  border: 1px solid %3; border-radius: 4px;"
                    "  padding: 6px 16px; font-size: 12px;"
                    "}"
                    "QPushButton:hover   { background: #313244; }"
                    "QPushButton:pressed { background: #45475a; }")
                .arg(C_SURFACE).arg(C_TEXT).arg(C_BORDER)
        );

        auto* saveBtn = new QPushButton("Save");
        saveBtn->setStyleSheet(
            QString("QPushButton {"
                    "  background: #1e3a5f; color: %1;"
                    "  border: 1px solid %2; border-radius: 4px;"
                    "  padding: 6px 24px; font-size: 12px; font-weight: 600;"
                    "}"
                    "QPushButton:hover   { background: #264d80; }"
                    "QPushButton:pressed { background: #1a3050; }")
                .arg(C_TEXT).arg(C_BLUE)
        );

        row->addWidget(m_grandTotalLabel);
        row->addStretch();
        row->addWidget(m_statusLabel);
        row->addWidget(clearBtn);
        row->addWidget(saveBtn);
        root->addLayout(row);

        connect(saveBtn,  &QPushButton::clicked, this, &EntryTab::onSave);
        connect(clearBtn, &QPushButton::clicked, this, &EntryTab::onClear);
        connect(m_dateEdit, &QDateEdit::dateChanged,
                this, &EntryTab::onDateChanged);
    }
}

QWidget* EntryTab::buildCategorySection(const QString& title,
                                         const QString& assetType,
                                         CategoryBlock& block)
{
    auto* frame = new QFrame;
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setObjectName("categoryFrame");
    frame->setStyleSheet(
        QString("QFrame#categoryFrame {"
                "  background: %1; border-radius: 8px;"
                "  border: 1px solid %2;"
                "}").arg(C_SURFACE).arg(C_BORDER)
    );

    auto* outer = new QVBoxLayout(frame);
    outer->setContentsMargins(14, 10, 14, 12);
    outer->setSpacing(8);

    // Header row: title + running total
    {
        auto* hdr = new QHBoxLayout;

        auto* titleLbl = new QLabel(title);
        titleLbl->setStyleSheet(
            QString("color: %1; font-size: 13px; font-weight: 700;"
                    "background: transparent; border: none;").arg(C_TEXT)
        );

        block.totalLabel = new QLabel(Fmt::money(0.0));
        block.totalLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: 600;"
                    "background: transparent; border: none;").arg(C_MUTED)
        );

        hdr->addWidget(titleLbl);
        hdr->addStretch();
        hdr->addWidget(block.totalLabel);
        outer->addLayout(hdr);
    }

    // Thin divider under the header
    {
        auto* sep = new QFrame;
        sep->setFixedHeight(1);
        sep->setStyleSheet(QString("background-color: %1;").arg(C_BORDER));
        outer->addWidget(sep);
    }

    // Dynamic row area
    auto* rowContainer = new QWidget;
    rowContainer->setStyleSheet("background: transparent;");
    block.rowLayout = new QVBoxLayout(rowContainer);
    block.rowLayout->setContentsMargins(0, 2, 0, 2);
    block.rowLayout->setSpacing(4);
    outer->addWidget(rowContainer);

    // Add button
    auto* addBtn = new QPushButton(QString("＋  Add %1").arg(assetType));
    addBtn->setStyleSheet(
        QString("QPushButton {"
                "  background: transparent; color: %1;"
                "  border: 1px dashed %2; border-radius: 4px;"
                "  padding: 5px 0; font-size: 12px;"
                "}"
                "QPushButton:hover   { background: %3; border-style: solid; }"
                "QPushButton:pressed { background: %2; }")
            .arg(C_BLUE).arg(C_BORDER).arg(C_MANTLE)
    );
    outer->addWidget(addBtn);

    connect(addBtn, &QPushButton::clicked, this, [this, &block]() {
        addRow(block);
    });

    return frame;
}

// ── Row management ────────────────────────────────────────────────────────────

void EntryTab::addRow(CategoryBlock& block,
                       const std::optional<Data::AssetEntry>& prefill)
{
    const bool interactive = !prefill.has_value();

    auto* container = new QWidget;
    container->setStyleSheet("background: transparent;");
    auto* lay = new QHBoxLayout(container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(block.assetType + " name");
    nameEdit->setStyleSheet(fieldStyle());

    auto* valueEdit = new QLineEdit;
    valueEdit->setPlaceholderText("0.00");
    valueEdit->setMaximumWidth(130);
    valueEdit->setStyleSheet(fieldStyle(C_GREEN));
    auto* v = new QDoubleValidator(0.0, 1e15, 6, valueEdit);
    v->setLocale(QLocale::c());
    valueEdit->setValidator(v);

    // ── Diff badge ────────────────────────────────────────────────────────────
    auto* diffBadge = new QLabel;
    diffBadge->setFixedWidth(72);
    diffBadge->setAlignment(Qt::AlignCenter);

    // Baseline: the value already stored for this asset (set when prefilling)
    const bool   hasBaseline    = prefill.has_value();
    const double baselineValue  = prefill ? prefill->value : 0.0;

    // ── Delete button ─────────────────────────────────────────────────────────
    auto* delBtn = new QPushButton("\xC3\x97");  // UTF-8 × (U+00D7)
    delBtn->setFixedSize(26, 26);
    delBtn->setToolTip("Remove this entry");
    delBtn->setStyleSheet(
        QString("QPushButton {"
                "  background: transparent; color: %1;"
                "  border: 1px solid %2; border-radius: 4px;"
                "  font-size: 14px; font-weight: 700;"
                "}"
                "QPushButton:hover { background: #3d2028; color: %3; border-color: %3; }")
            .arg(C_MUTED).arg(C_BORDER).arg(C_RED)
    );

    // Pre-fill if provided
    if (prefill) {
        nameEdit->setText(prefill->name);
        if (prefill->value != 0.0)
            valueEdit->setText(QLocale::c().toString(prefill->value, 'f', 2));
    }

    // Initialise the badge with the current (pre-filled) text
    refreshDiffBadge(diffBadge, hasBaseline, baselineValue, valueEdit->text());

    lay->addWidget(nameEdit, /*stretch=*/1);
    lay->addWidget(valueEdit);
    lay->addWidget(diffBadge);
    lay->addWidget(delBtn);

    block.rowLayout->addWidget(container);
    block.rows.append(AssetRow{container, nameEdit, valueEdit,
                                diffBadge, baselineValue, hasBaseline});

    // Live-update the badge and the category totals while the user types.
    connect(valueEdit, &QLineEdit::textChanged, diffBadge,
            [diffBadge, hasBaseline, baselineValue](const QString& text) {
                EntryTab::refreshDiffBadge(diffBadge, hasBaseline, baselineValue, text);
            });
    connect(valueEdit, &QLineEdit::textChanged, this, &EntryTab::updateTotals);

    connect(delBtn, &QPushButton::clicked, this, [this, &block, container]() {
        removeRow(block, container);
    });

    updateTotals();

    if (interactive)
        nameEdit->setFocus();
}

void EntryTab::removeRow(CategoryBlock& block, QWidget* rowWidget)
{
    for (int i = 0; i < block.rows.size(); ++i) {
        if (block.rows[i].container == rowWidget) {
            block.rows.remove(i);
            break;
        }
    }
    block.rowLayout->removeWidget(rowWidget);
    rowWidget->hide();
    rowWidget->deleteLater();   // defer — safe while clicked signal unwinds
    updateTotals();
}

void EntryTab::clearBlock(CategoryBlock& block)
{
    const QVector<AssetRow> snapshot = block.rows;  // copy before modifying
    for (const AssetRow& r : snapshot)
        removeRow(block, r.container);
}

void EntryTab::clearAllRows()
{
    clearBlock(m_stocks);
    clearBlock(m_coins);
    clearBlock(m_metals);
    clearBlock(m_etfs);
}

// ── Snapshot ↔ form ───────────────────────────────────────────────────────────

void EntryTab::fillFromSnapshot(const Data::FinancialSnapshot& snap)
{
    clearAllRows();
    for (const Data::AssetEntry& e : snap.stocks) addRow(m_stocks, e);
    for (const Data::AssetEntry& e : snap.coins)  addRow(m_coins,  e);
    for (const Data::AssetEntry& e : snap.metals) addRow(m_metals, e);
    for (const Data::AssetEntry& e : snap.etfs)   addRow(m_etfs,   e);
    updateTotals();
}

QVector<Data::AssetEntry> EntryTab::collectEntries(const CategoryBlock& block) const
{
    QVector<Data::AssetEntry> result;
    for (const AssetRow& row : block.rows) {
        const QString name = row.nameEdit->text().trimmed();
        bool ok = false;
        const double val = QLocale::c().toDouble(row.valueEdit->text(), &ok);
        if (!name.isEmpty() || ok)
            result.append({name.isEmpty() ? "(unnamed)" : name, ok ? val : 0.0});
    }
    return result;
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void EntryTab::onDateChanged(const QDate& date)
{
    if (m_loadingData) return;

    const auto snap = Database::instance().loadSnapshot(date);
    if (snap)
        fillFromSnapshot(*snap);
    else
        clearAllRows();   // no entry for this date — show a blank form
}

void EntryTab::onSave()
{
    Data::FinancialSnapshot snap;
    snap.date    = m_dateEdit->date();
    snap.stocks  = collectEntries(m_stocks);
    snap.coins   = collectEntries(m_coins);
    snap.metals  = collectEntries(m_metals);
    snap.etfs    = collectEntries(m_etfs);

    if (Database::instance().saveSnapshot(snap))
        setStatus("✓  Saved");
    else
        setStatus("⚠  Save failed — " + Database::instance().lastError(),
                  /*isError=*/true);
}

void EntryTab::onClear()
{
    clearAllRows();
    m_dateEdit->setDate(QDate::currentDate());
    updateTotals();
}

void EntryTab::updateTotals()
{
    auto sumBlock = [](const CategoryBlock& b) -> double {
        double t = 0.0;
        for (const AssetRow& r : b.rows) {
            bool ok = false;
            const double v = QLocale::c().toDouble(r.valueEdit->text(), &ok);
            if (ok) t += v;
        }
        return t;
    };

    const double sTotal = sumBlock(m_stocks);
    const double cTotal = sumBlock(m_coins);
    const double mTotal = sumBlock(m_metals);
    const double eTotal = sumBlock(m_etfs);

    auto setTotal = [](CategoryBlock& b, double v) {
        if (b.totalLabel) b.totalLabel->setText(Fmt::money(v));
    };
    setTotal(m_stocks, sTotal);
    setTotal(m_coins,  cTotal);
    setTotal(m_metals, mTotal);
    setTotal(m_etfs,   eTotal);

    if (m_grandTotalLabel)
        m_grandTotalLabel->setText(
            QString("Grand Total:  %1").arg(Fmt::money(sTotal + cTotal + mTotal + eTotal))
        );
}

// ── Diff badge ────────────────────────────────────────────────────────────────

/*static*/ void EntryTab::refreshDiffBadge(QLabel*        badge,
                                             bool           hasBaseline,
                                             double         baseline,
                                             const QString& currentText)
{
    // Grey "---%" — no baseline or zero baseline (division by zero)
    if (!hasBaseline || baseline == 0.0) {
        badge->setText("--.-% ");
        badge->setStyleSheet(
            QString("color: %1; background: transparent;"
                    "border-radius: 4px; padding: 2px 5px; font-size: 11px;")
                .arg(C_MUTED)
        );
        return;
    }

    bool ok = false;
    const double newVal = QLocale::c().toDouble(currentText, &ok);

    if (!ok || currentText.trimmed().isEmpty()) {
        badge->setText("--.-% ");
        badge->setStyleSheet(
            QString("color: %1; background: transparent;"
                    "border-radius: 4px; padding: 2px 5px; font-size: 11px;")
                .arg(C_MUTED)
        );
        return;
    }

    const double pct = (newVal - baseline) / baseline * 100.0;

    if (pct > 0.0) {
        badge->setText(QString("+%1%").arg(pct, 0, 'f', 1));
        badge->setStyleSheet(
            "color: #a6e3a1; background: #1a3328;"
            "border: 1px solid #2d5a45; border-radius: 4px;"
            "padding: 2px 5px; font-size: 11px; font-weight: 600;"
        );
    } else if (pct < 0.0) {
        badge->setText(QString("%1%").arg(pct, 0, 'f', 1));
        badge->setStyleSheet(
            "color: #f38ba8; background: #3d1a28;"
            "border: 1px solid #5a2d3d; border-radius: 4px;"
            "padding: 2px 5px; font-size: 11px; font-weight: 600;"
        );
    } else {
        badge->setText("0.0%");
        badge->setStyleSheet(
            QString("color: %1; background: transparent;"
                    "border-radius: 4px; padding: 2px 5px; font-size: 11px;")
                .arg(C_MUTED)
        );
    }
}

void EntryTab::setStatus(const QString& msg, bool isError)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("font-size: 11px; padding: 0 4px; color: %1;")
            .arg(isError ? C_RED : C_GREEN)
    );
    QTimer::singleShot(3000, this, [this]() { m_statusLabel->clear(); });
}
