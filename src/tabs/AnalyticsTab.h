#pragma once

#include <QWidget>

class QChart;
class QChartView;
class QLabel;

class AnalyticsTab : public QWidget
{
    Q_OBJECT

public:
    explicit AnalyticsTab(QWidget* parent = nullptr);

private:
    void buildUi();
    void loadChart();

    QChartView* m_chartView{nullptr};
    QChart*     m_chart{nullptr};
    QLabel*     m_emptyLabel{nullptr};
};
