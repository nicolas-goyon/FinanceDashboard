#pragma once

#include <QWidget>

class QTableWidget;

class PortfolioTab : public QWidget
{
    Q_OBJECT

public:
    explicit PortfolioTab(QWidget* parent = nullptr);

private:
    void buildUi();
    void populateTable();

    QTableWidget* m_table{nullptr};
};
