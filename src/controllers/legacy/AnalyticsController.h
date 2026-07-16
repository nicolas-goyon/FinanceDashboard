#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── AnalyticsController ───────────────────────────────────────────────────────
//
// C++ backend for qml/tabs/AnalyticsTab.qml (QML_ELEMENT, instantiated in QML).
//
// Loads every stored snapshot from the Database and computes the cumulative
// stacked-area boundaries. All math lives here — QML only appends the
// pre-computed points to LineSeries and renders AreaSeries between them.
//
// Layer order (bottom → top): ETFs, Metals, Coins, Stocks.
// Boundary k = cumulative sum of layers 1..k, so:
//   etfsBound   = etfs
//   metalsBound = etfs + metals
//   coinsBound  = etfs + metals + coins
//   stocksBound = etfs + metals + coins + stocks   (= total)

class AnalyticsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool   hasData   READ hasData   NOTIFY dataChanged)
    Q_PROPERTY(double maxTotal  READ maxTotal  NOTIFY dataChanged)  // Y axis max (padded)
    Q_PROPERTY(double minTimeMs READ minTimeMs NOTIFY dataChanged)  // X axis min (ms since epoch)
    Q_PROPERTY(double maxTimeMs READ maxTimeMs NOTIFY dataChanged)  // X axis max (ms since epoch)

public:
    explicit AnalyticsController(QObject* parent = nullptr);

    bool   hasData()   const { return m_hasData; }
    double maxTotal()  const { return m_maxTotal; }
    double minTimeMs() const { return m_minTimeMs; }
    double maxTimeMs() const { return m_maxTimeMs; }

    // Re-read all snapshots from the database and recompute the layers.
    Q_INVOKABLE void reload();

    // X values — snapshot dates as ms since epoch (ascending, one per snapshot).
    Q_INVOKABLE QVariantList timestamps() const { return m_timestamps; }

    // Cumulative upper boundaries — same length/order as timestamps().
    Q_INVOKABLE QVariantList etfsBound()   const { return m_etfsBound; }
    Q_INVOKABLE QVariantList metalsBound() const { return m_metalsBound; }
    Q_INVOKABLE QVariantList coinsBound()  const { return m_coinsBound; }
    Q_INVOKABLE QVariantList stocksBound() const { return m_stocksBound; }

signals:
    void dataChanged();

private:
    bool   m_hasData   = false;
    double m_maxTotal  = 1000.0;
    double m_minTimeMs = 0.0;
    double m_maxTimeMs = 0.0;

    QVariantList m_timestamps;
    QVariantList m_etfsBound;
    QVariantList m_metalsBound;
    QVariantList m_coinsBound;
    QVariantList m_stocksBound;
};
