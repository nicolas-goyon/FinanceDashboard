#include "OverviewController.h"
#include "data/DebugData.h"
#include "utils/Formatter.h"

namespace {

QVariantMap card(const QString& label, const QString& value,
                 const QString& color = QStringLiteral("#cdd6f4"))
{
    return { { "label", label }, { "value", value }, { "color", color } };
}

} // namespace

OverviewController::OverviewController(QObject* parent)
    : QObject(parent)
{
    // MOCK: sample summary from data/DebugData.h
    const auto s = Debug::sampleSummary();

    const QString dayColor = s.dayChange    >= 0 ? "#a6e3a1" : "#f38ba8";
    const QString pctColor = s.dayChangePct >= 0 ? "#a6e3a1" : "#f38ba8";

    m_cards = {
        card("Total Value",    Fmt::money(s.totalValue)),
        card("Total Cost",     Fmt::money(s.totalCost)),
        card("Unrealized P&L", Fmt::money(s.unrealizedPnL, true), "#a6e3a1"),
        card("Realized P&L",   Fmt::money(s.realizedPnL,   true), "#a6e3a1"),
        card("Day Change",     Fmt::money(s.dayChange,     true), dayColor),
        card("Day Change %",
             QString("%1%2%").arg(s.dayChangePct >= 0 ? "+" : "")
                             .arg(s.dayChangePct, 0, 'f', 2),
             pctColor),
    };

    // MOCK: static debug list — same as the legacy tab
    m_movers = {
        QVariantMap{ { "text", "▲  NVDA  +4.82%   $875.30" }, { "up", true  } },
        QVariantMap{ { "text", "▲  MSFT  +1.95%   $415.20" }, { "up", true  } },
        QVariantMap{ { "text", "▼  TSLA  -0.73%   $248.70" }, { "up", false } },
    };
}
