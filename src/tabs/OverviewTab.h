#pragma once

#include <QWidget>

class QLabel;

class OverviewTab : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewTab(QWidget* parent = nullptr);

private:
    void buildUi();
    QLabel* makeStatCard(const QString& label, const QString& value, const QString& color = "#ffffff");
};
