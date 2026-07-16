#include "MainWindow.h"
#include "tabs/OverviewTab.h"
#include "tabs/PortfolioTab.h"
#include "tabs/AnalyticsTab.h"
#include "tabs/ImportTab.h"
#include "tabs/EntryTab.h"
#include "tabs/DatabaseTab.h"

#include <QTabWidget>
#include <QStatusBar>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    applyTheme();
    buildUi();
    setWindowTitle("Finance Dashboard  [DEBUG]");
    resize(1100, 700);
}

void MainWindow::applyTheme()
{
    // Catppuccin-inspired dark palette applied globally via stylesheet
    qApp->setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #11111b;
            color: #cdd6f4;
            font-family: "Segoe UI", "Inter", sans-serif;
            font-size: 13px;
        }
        QTabWidget::pane {
            border: 1px solid #313244;
            border-radius: 4px;
        }
        QTabBar::tab {
            background: #1e1e2e;
            color: #6c7086;
            padding: 8px 20px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            font-weight: 600;
        }
        QTabBar::tab:selected {
            background: #313244;
            color: #cdd6f4;
        }
        QTabBar::tab:hover {
            background: #24273a;
            color: #b4befe;
        }
        QScrollBar:vertical {
            background: #1e1e2e;
            width: 8px;
        }
        QScrollBar::handle:vertical {
            background: #45475a;
            border-radius: 4px;
        }
        QStatusBar {
            background: #181825;
            color: #6c7086;
            font-size: 11px;
        }
    )");
}

void MainWindow::buildUi()
{
    m_tabs = new QTabWidget(this);

    m_overviewTab  = new OverviewTab(m_tabs);
    m_portfolioTab = new PortfolioTab(m_tabs);
    m_analyticsTab = new AnalyticsTab(m_tabs);
    m_importTab    = new ImportTab(m_tabs);
    m_entryTab     = new EntryTab(m_tabs);
    m_databaseTab  = new DatabaseTab(m_tabs);

    m_tabs->addTab(m_overviewTab,  "Overview");
    m_tabs->addTab(m_portfolioTab, "Portfolio");
    m_tabs->addTab(m_analyticsTab, "Analytics");
    m_tabs->addTab(m_importTab,    "Import");
    m_tabs->addTab(m_entryTab,     "Entry");
    m_tabs->addTab(m_databaseTab,  "Database");

    setCentralWidget(m_tabs);
    statusBar()->showMessage("Finance Dashboard  |  Debug mode  |  Data is not live");
}
