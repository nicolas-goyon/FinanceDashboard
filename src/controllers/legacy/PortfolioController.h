#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── PortfolioController ───────────────────────────────────────────────────────
//
// C++ backend for qml/tabs/PortfolioTab.qml.
// Still fed by Debug::sampleHoldings() (like the legacy tab). All P&L math
// and money formatting is done here.

class PortfolioController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ ticker, name, shares, avgCost, price, marketValue, pnl, pnlPct, positive }]
    // — every value pre-formatted for display
    Q_PROPERTY(QVariantList rows READ rows CONSTANT)
    Q_PROPERTY(QString footerText READ footerText CONSTANT)

public:
    explicit PortfolioController(QObject* parent = nullptr);

    QVariantList rows()      const { return m_rows; }
    QString     footerText() const { return m_footerText; }

private:
    QVariantList m_rows;
    QString      m_footerText;
};
