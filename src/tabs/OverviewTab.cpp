#include "OverviewTab.h"
#include "data/DebugData.h"
#include "utils/Formatter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QGroupBox>

OverviewTab::OverviewTab(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static QFrame* separator()
{
    auto* f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Sunken);
    return f;
}

QLabel* OverviewTab::makeStatCard(const QString& label,
                                  const QString& value,
                                  const QString& color)
{
    auto* card  = new QFrame;
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet(QString(
        "QFrame { background: #1e1e2e; border-radius: 8px; padding: 12px; }"
    ));

    auto* lay  = new QVBoxLayout(card);
    auto* lbl  = new QLabel(label);
    auto* val  = new QLabel(value);

    lbl->setStyleSheet("color: #888; font-size: 11px; font-weight: 500;");
    val->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: 700;").arg(color));

    lay->addWidget(lbl);
    lay->addWidget(val);

    // Wrap in a container that returns the card widget; we return the value label
    // so callers could update it later. For now return a dummy.
    (void)card;  // card is owned by the layout below via addWidget
    return val;  // unused by caller for now — kept for future live-update hook
}

// ── UI build ─────────────────────────────────────────────────────────────────

void OverviewTab::buildUi()
{
    const auto& s = Debug::sampleSummary();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* title = new QLabel("Portfolio Overview");
    title->setStyleSheet("font-size: 20px; font-weight: 700; color: #cdd6f4;");
    root->addWidget(title);

    auto* subtitle = new QLabel("[DEBUG DATA — not live]");
    subtitle->setStyleSheet("color: #f38ba8; font-size: 11px;");
    root->addWidget(subtitle);

    root->addWidget(separator());

    // ── Stat cards row ────────────────────────────────────────────────────────
    auto* grid = new QGridLayout;
    grid->setSpacing(12);

    auto addCard = [&](int row, int col,
                       const QString& lbl, const QString& val, const QString& clr = "#cdd6f4")
    {
        auto* frame = new QFrame;
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setStyleSheet("QFrame { background: #1e1e2e; border-radius: 8px; }");
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(14, 10, 14, 10);

        auto* l = new QLabel(lbl);
        l->setStyleSheet("color: #6c7086; font-size: 11px; font-weight: 600; text-transform: uppercase;");

        auto* v = new QLabel(val);
        v->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700;").arg(clr));

        fl->addWidget(l);
        fl->addWidget(v);
        grid->addWidget(frame, row, col);
    };

    addCard(0, 0, "Total Value",      Fmt::money(s.totalValue));
    addCard(0, 1, "Total Cost",       Fmt::money(s.totalCost));
    addCard(0, 2, "Unrealized P&L",   Fmt::money(s.unrealizedPnL, /*showPlus=*/true), "#a6e3a1");
    addCard(1, 0, "Realized P&L",     Fmt::money(s.realizedPnL,   /*showPlus=*/true), "#a6e3a1");
    addCard(1, 1, "Day Change",
            Fmt::money(s.dayChange, /*showPlus=*/true),
            s.dayChange >= 0 ? "#a6e3a1" : "#f38ba8");
    addCard(1, 2, "Day Change %",
            QString("%1%2%").arg(s.dayChangePct >= 0 ? "+" : "")
                            .arg(s.dayChangePct, 0, 'f', 2),
            s.dayChangePct >= 0 ? "#a6e3a1" : "#f38ba8");

    root->addLayout(grid);

    root->addWidget(separator());

    // ── Top movers (static debug list) ────────────────────────────────────────
    auto* moversBox = new QGroupBox("Top Movers  [debug]");
    moversBox->setStyleSheet(
        "QGroupBox { color: #cdd6f4; font-weight: 600; border: 1px solid #313244; "
        "border-radius: 6px; margin-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
    );
    auto* moverLayout = new QVBoxLayout(moversBox);

    const QStringList movers = {
        "▲  NVDA  +4.82%   $875.30",
        "▲  MSFT  +1.95%   $415.20",
        "▼  TSLA  -0.73%   $248.70",
    };
    for (const auto& m : movers) {
        auto* lbl = new QLabel(m);
        bool up = m.startsWith("▲");
        lbl->setStyleSheet(QString("color: %1; font-family: monospace; font-size: 13px;")
                           .arg(up ? "#a6e3a1" : "#f38ba8"));
        moverLayout->addWidget(lbl);
    }

    root->addWidget(moversBox);
    root->addStretch();
}
