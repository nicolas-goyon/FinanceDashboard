#pragma once

#include <QDate>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── ProjectsController ────────────────────────────────────────────────────────
//
// Backend for qml/pages/ProjectsPage.qml (PROJECT.md page 5 — Projects).
// Projects are MOCK; the loan simulator uses the real annuity formula
// (fixed-rate amortization) computed in C++.

class ProjectsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QStringList projectNames READ projectNames NOTIFY projectsChanged)
    // [{ name, targetText, earmarkedText, monthlyText, completionText,
    //    targetDateText, progressPct, gapText, onTrack }] — list-page cards
    Q_PROPERTY(QVariantList projectCards READ projectCards NOTIFY projectsChanged)
    Q_PROPERTY(int currentProject READ currentProject WRITE setCurrentProject NOTIFY projectChanged)

    // [{ label, value, sub, color }] — amount needed, projected completion, gap
    Q_PROPERTY(QVariantList kpis READ kpis NOTIFY projectChanged)
    Q_PROPERTY(double targetAmount READ targetAmount NOTIFY projectChanged)
    Q_PROPERTY(double chartMax   READ chartMax   NOTIFY projectChanged)
    Q_PROPERTY(double minTimeMs  READ minTimeMs  NOTIFY projectChanged)
    Q_PROPERTY(double maxTimeMs  READ maxTimeMs  NOTIFY projectChanged)

    // Loan simulator — annuity math, real (not mock)
    Q_PROPERTY(double loanRatePct READ loanRatePct WRITE setLoanRatePct NOTIFY loanChanged)
    // [{ label, monthly, totalInterest, totalCost }] — 10 / 15 / 20 year scenarios
    Q_PROPERTY(QVariantList loanScenarios READ loanScenarios NOTIFY loanChanged)
    Q_PROPERTY(QString saveVsBorrowText READ saveVsBorrowText NOTIFY loanChanged)

public:
    explicit ProjectsController(QObject* parent = nullptr);

    QStringList projectNames() const { return m_projectNames; }
    QVariantList projectCards() const { return m_projectCards; }
    int  currentProject() const { return m_currentProject; }
    void setCurrentProject(int index);

    // Create a new project (mock — not persisted). monthsToTarget from today.
    Q_INVOKABLE void addProject(const QString& name, double target,
                                int monthsToTarget, double earmarked,
                                double monthly);

    QVariantList kpis() const { return m_kpis; }
    double targetAmount() const { return m_targetAmount; }
    double chartMax()  const { return m_chartMax; }
    double minTimeMs() const { return m_minTimeMs; }
    double maxTimeMs() const { return m_maxTimeMs; }

    double loanRatePct() const { return m_loanRatePct; }
    void setLoanRatePct(double pct);
    QVariantList loanScenarios() const { return m_loanScenarios; }
    QString saveVsBorrowText() const { return m_saveVsBorrowText; }

    // Projected savings line (x = ms since epoch, y = €)
    Q_INVOKABLE QVariantList projectionTimestamps() const { return m_projTimestamps; }
    Q_INVOKABLE QVariantList projectionValues()     const { return m_projValues; }

signals:
    void projectsChanged();   // list of projects changed (add / rename)
    void projectChanged();    // selected project or its computed data changed
    void loanChanged();

private:
    struct Project {
        QString name;
        double  target;
        QDate   targetDate;
        double  earmarked;
        double  monthly;
    };

    void recomputeProject();
    void recomputeLoan();
    void rebuildCards();

    QVector<Project> m_projects;
    QStringList m_projectNames;
    QVariantList m_projectCards;
    int m_currentProject = 0;

    QVariantList m_kpis, m_projTimestamps, m_projValues;
    double m_targetAmount = 0, m_chartMax = 0, m_minTimeMs = 0, m_maxTimeMs = 0;
    double m_fundingGap = 0;

    double m_loanRatePct = 3.5;
    QVariantList m_loanScenarios;
    QString m_saveVsBorrowText;
};
