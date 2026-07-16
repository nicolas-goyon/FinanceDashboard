#include "AnalyticsController.h"
#include "db/Database.h"

#include <algorithm>

#include <QDate>
#include <QDateTime>
#include <QVector>

AnalyticsController::AnalyticsController(QObject* parent)
    : QObject(parent)
{
    reload();
}

void AnalyticsController::reload()
{
    m_timestamps.clear();
    m_etfsBound.clear();
    m_metalsBound.clear();
    m_coinsBound.clear();
    m_stocksBound.clear();
    m_hasData  = false;
    m_maxTotal = 1000.0;

    // ── Load snapshots (oldest → newest) ──────────────────────────────────────
    QVector<QDate> dates = Database::instance().listDates(); // newest first
    std::sort(dates.begin(), dates.end());

    double peak = 0.0;

    for (const QDate& d : dates) {
        const auto snap = Database::instance().loadSnapshot(d);
        if (!snap) continue;

        double etfs = 0, metals = 0, coins = 0, stocks = 0;
        for (const auto& e : snap->etfs)   etfs   += e.value;
        for (const auto& e : snap->metals) metals += e.value;
        for (const auto& e : snap->coins)  coins  += e.value;
        for (const auto& e : snap->stocks) stocks += e.value;

        // Cumulative boundaries, bottom → top: ETFs, Metals, Coins, Stocks
        const double b1 = etfs;
        const double b2 = b1 + metals;
        const double b3 = b2 + coins;
        const double b4 = b3 + stocks;   // total

        m_timestamps  .append(double(d.startOfDay().toMSecsSinceEpoch()));
        m_etfsBound   .append(b1);
        m_metalsBound .append(b2);
        m_coinsBound  .append(b3);
        m_stocksBound .append(b4);

        peak = std::max(peak, b4);
    }

    if (!m_timestamps.isEmpty()) {
        m_hasData = true;

        // Y axis: 12 % headroom above the peak (min 1000 so an all-zero
        // snapshot still shows sensible axes).
        m_maxTotal = peak > 0 ? peak * 1.12 : 1000.0;

        // X axis range. A single snapshot needs a non-zero span, so pad ±12 h.
        m_minTimeMs = m_timestamps.first().toDouble();
        m_maxTimeMs = m_timestamps.last().toDouble();
        if (qFuzzyCompare(m_minTimeMs, m_maxTimeMs)) {
            constexpr double kHalfDayMs = 12.0 * 60 * 60 * 1000;
            m_minTimeMs -= kHalfDayMs;
            m_maxTimeMs += kHalfDayMs;
        }
    } else {
        m_minTimeMs = 0.0;
        m_maxTimeMs = 0.0;
    }

    emit dataChanged();
}
