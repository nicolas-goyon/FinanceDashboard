#pragma once

#include <QMainWindow>

class QTabWidget;
class OverviewTab;
class PortfolioTab;
class AnalyticsTab;
class ImportTab;
class EntryTab;
class DatabaseTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void buildUi();
    void applyTheme();

    QTabWidget*   m_tabs{nullptr};
    OverviewTab*  m_overviewTab{nullptr};
    PortfolioTab* m_portfolioTab{nullptr};
    AnalyticsTab* m_analyticsTab{nullptr};
    ImportTab*    m_importTab{nullptr};
    EntryTab*     m_entryTab{nullptr};
    DatabaseTab*  m_databaseTab{nullptr};
};
