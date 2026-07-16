#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqml.h>

// ── OverviewController ────────────────────────────────────────────────────────
//
// C++ backend for qml/tabs/OverviewTab.qml.
// Still fed by Debug::sampleSummary() (like the legacy tab) — swap the data
// source here when wiring to the real Database.

class OverviewController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ label, value, color }] — pre-formatted stat cards (2×3 grid)
    Q_PROPERTY(QVariantList cards READ cards CONSTANT)
    // [{ text, up }] — top movers list
    Q_PROPERTY(QVariantList movers READ movers CONSTANT)

public:
    explicit OverviewController(QObject* parent = nullptr);

    QVariantList cards()  const { return m_cards; }
    QVariantList movers() const { return m_movers; }

private:
    QVariantList m_cards;
    QVariantList m_movers;
};
