#pragma once

#include "models/PortfolioData.h"

#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QScrollArea;

class ImportTab : public QWidget
{
    Q_OBJECT

public:
    explicit ImportTab(QWidget* parent = nullptr);

private slots:
    void onBrowse();
    void onImport();

private:
    void buildUi();

    /// Rebuilds the scroll area to show parsed PortfolioSummary data.
    void showResults(const Data::PortfolioSummary& summary);

    /// Rebuilds the scroll area to show an error or idle placeholder.
    void showMessage(const QString& msg, bool isError = false);

    /// Replaces m_resultsWidget with a fresh empty widget inside the scroll area.
    QWidget* resetResultsWidget();

    // ── Persistent widgets ────────────────────────────────────────────────────
    QLineEdit*   m_pathEdit{nullptr};
    QPushButton* m_importBtn{nullptr};
    QLabel*      m_statusLabel{nullptr};
    QScrollArea* m_scroll{nullptr};
    QWidget*     m_resultsWidget{nullptr};
};
