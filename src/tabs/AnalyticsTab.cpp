#include "AnalyticsTab.h"
#include "db/Database.h"

#include <algorithm>

#include <QVBoxLayout>
#include <QLabel>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCategoryAxis>

// ── Colours ───────────────────────────────────────────────────────────────────
namespace {

// Fill colours (semi-transparent)
const QColor kStocksFill  {137, 180, 250, 110};   // #89b4fa ~43%
const QColor kCoinsFill   {243, 139, 168, 110};   // #f38ba8
const QColor kMetalsFill  {249, 226, 175, 110};   // #f9e2af
const QColor kEtfsFill    {166, 227, 161, 110};   // #a6e3a1

// Border/pen colours (opaque)
const QColor kStocksBorder {"#89b4fa"};
const QColor kCoinsBorder  {"#f38ba8"};
const QColor kMetalsBorder {"#f9e2af"};
const QColor kEtfsBorder   {"#a6e3a1"};

} // namespace

// ── AnalyticsTab ──────────────────────────────────────────────────────────────

AnalyticsTab::AnalyticsTab(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    loadChart();
}

void AnalyticsTab::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

    // Header
    auto* title = new QLabel("Analytics");
    title->setStyleSheet("font-size: 20px; font-weight: 700; color: #cdd6f4;");
    root->addWidget(title);

    // Chart placeholder / empty state
    m_emptyLabel = new QLabel("No snapshots recorded yet.\nSave entries in the Entry tab to see the chart.");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(
        "color: #6c7086; font-size: 13px; background: #1e1e2e;"
        "border: 1px solid #313244; border-radius: 8px; padding: 40px;"
    );
    m_emptyLabel->setVisible(false);
    root->addWidget(m_emptyLabel, /*stretch=*/1);

    // Chart
    m_chart = new QChart;
    m_chart->setTitle("Portfolio Value Over Time");
    m_chart->setTitleBrush(QColor("#cdd6f4"));
    m_chart->setBackgroundBrush(QColor("#1e1e2e"));
    m_chart->setPlotAreaBackgroundBrush(QColor("#181825"));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->legend()->setLabelColor(QColor("#cdd6f4"));
    m_chart->legend()->setBackgroundVisible(false);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(320);
    m_chartView->setStyleSheet("background: #1e1e2e; border-radius: 8px;");
    root->addWidget(m_chartView, /*stretch=*/1);
}

void AnalyticsTab::loadChart()
{
    // ── Load snapshots ────────────────────────────────────────────────────────
    QVector<QDate> dates = Database::instance().listDates(); // newest first
    if (dates.isEmpty()) {
        m_chartView->setVisible(false);
        m_emptyLabel->setVisible(true);
        return;
    }

    // Sort ascending (oldest → newest)
    std::sort(dates.begin(), dates.end());

    struct Totals { double stocks{0}, coins{0}, metals{0}, etfs{0}; };
    QVector<Totals>  pts;
    QVector<QDate>   validDates;

    for (const QDate& d : dates) {
        const auto snap = Database::instance().loadSnapshot(d);
        if (!snap) continue;
        Totals t;
        for (const auto& e : snap->stocks) t.stocks += e.value;
        for (const auto& e : snap->coins)  t.coins  += e.value;
        for (const auto& e : snap->metals) t.metals += e.value;
        for (const auto& e : snap->etfs)   t.etfs   += e.value;
        pts.append(t);
        validDates.append(d);
    }

    if (pts.isEmpty()) {
        m_chartView->setVisible(false);
        m_emptyLabel->setVisible(true);
        return;
    }

    m_chart->removeAllSeries();
    const auto oldAxes = m_chart->axes();
    for (auto* ax : oldAxes) m_chart->removeAxis(ax);

    // ── Build cumulative layers ───────────────────────────────────────────────
    // Layer indices:
    //   0 = zero baseline
    //   1 = etfs
    //   2 = etfs + metals
    //   3 = etfs + metals + coins
    //   4 = etfs + metals + coins + stocks   (total)

    const int n = pts.size();
    QVector<QVector<QPointF>> layers(5);
    for (int i = 0; i < n; ++i) {
        const Totals& t = pts[i];
        layers[0].append({(double)i, 0.0});
        layers[1].append({(double)i, t.etfs});
        layers[2].append({(double)i, t.etfs + t.metals});
        layers[3].append({(double)i, t.etfs + t.metals + t.coins});
        layers[4].append({(double)i, t.etfs + t.metals + t.coins + t.stocks});
    }

    // Each QAreaSeries owns its upper/lower QLineSeries,
    // so we create fresh series for every boundary pair.
    auto makeLine = [&](int layerIdx) -> QLineSeries* {
        auto* s = new QLineSeries;
        for (const auto& p : layers[layerIdx]) s->append(p);
        return s;
    };

    auto makeArea = [&](int upper, int lower,
                        const QString& name,
                        const QColor& fill,
                        const QColor& border) -> QAreaSeries*
    {
        auto* upperLine = makeLine(upper);
        auto* lowerLine = makeLine(lower);

        // Draw a dot at every data point (essential for single-snapshot view).
        upperLine->setPointsVisible(true);
        upperLine->setMarkerSize(7.0);
        upperLine->setPen(QPen(border, 1.5));

        auto* area = new QAreaSeries(upperLine, lowerLine);
        area->setName(name);
        area->setBrush(fill);
        area->setPen(QPen(border, 1.5));
        return area;
    };

    // Bottom → top: ETFs, Metals, Coins, Stocks
    auto* areaEtfs   = makeArea(1, 0, "ETFs",   kEtfsFill,   kEtfsBorder);
    auto* areaMetals = makeArea(2, 1, "Metals", kMetalsFill, kMetalsBorder);
    auto* areaCoins  = makeArea(3, 2, "Coins",  kCoinsFill,  kCoinsBorder);
    auto* areaStocks = makeArea(4, 3, "Stocks", kStocksFill, kStocksBorder);

    m_chart->addSeries(areaEtfs);
    m_chart->addSeries(areaMetals);
    m_chart->addSeries(areaCoins);
    m_chart->addSeries(areaStocks);

    // ── X axis ────────────────────────────────────────────────────────────────
    auto* axisX = new QCategoryAxis;
    axisX->setLabelsColor(QColor("#6c7086"));
    axisX->setGridLineColor(QColor("#313244"));
    axisX->setLinePenColor(QColor("#313244"));
    // Show a label for every date (or thin them if many)
    for (int i = 0; i < validDates.size(); ++i)
        axisX->append(validDates[i].toString("d MMM yy"), i);
    // Guard: a single point needs a non-zero range so the dot is visible.
    axisX->setRange(0, qMax(n - 1, 1));
    m_chart->addAxis(axisX, Qt::AlignBottom);

    // ── Y axis ────────────────────────────────────────────────────────────────
    double maxY = 0;
    for (const auto& t : pts)
        maxY = qMax(maxY, t.stocks + t.coins + t.metals + t.etfs);

    auto* axisY = new QValueAxis;
    axisY->setLabelsColor(QColor("#6c7086"));
    axisY->setGridLineColor(QColor("#313244"));
    axisY->setLinePenColor(QColor("#313244"));
    axisY->setLabelFormat("$%.0f");
    // Guard: non-zero Y range so a single all-zero snapshot still shows axes.
    axisY->setRange(0, maxY > 0 ? maxY * 1.12 : 1000);
    axisY->setTickCount(6);
    m_chart->addAxis(axisY, Qt::AlignLeft);

    for (auto* s : m_chart->series()) {
        s->attachAxis(axisX);
        s->attachAxis(axisY);
    }

    m_chartView->setVisible(true);
    m_emptyLabel->setVisible(false);
}
