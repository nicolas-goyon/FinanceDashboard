#include "DataEntryController.h"

#include <QTimer>

DataEntryController::DataEntryController(QObject* parent)
    : QObject(parent)
{
    // MOCK: transaction history rows
    m_historyRows = {
        QVariantMap{ { "date", "2026-07-03" }, { "label", "Carrefour"        }, { "amount", "-86,40 €"  }, { "category", "Groceries" } },
        QVariantMap{ { "date", "2026-07-01" }, { "label", "Monthly rent"     }, { "amount", "-950,00 €" }, { "category", "Rent"      } },
        QVariantMap{ { "date", "2026-06-28" }, { "label", "SNCF ticket"      }, { "amount", "-42,00 €"  }, { "category", "Transport" } },
        QVariantMap{ { "date", "2026-06-25" }, { "label", "EDF electricity"  }, { "amount", "-118,50 €" }, { "category", "Utilities" } },
        QVariantMap{ { "date", "2026-06-21" }, { "label", "Cinema + dinner"  }, { "amount", "-64,90 €"  }, { "category", "Leisure"   } },
        QVariantMap{ { "date", "2026-06-15" }, { "label", "Salary"           }, { "amount", "+3 380,00 €" }, { "category", "—"       } },
        QVariantMap{ { "date", "2026-06-12" }, { "label", "Pharmacy"         }, { "amount", "-23,70 €"  }, { "category", "Other"     } },
        QVariantMap{ { "date", "2026-06-08" }, { "label", "Lidl"             }, { "amount", "-54,20 €"  }, { "category", "Groceries" } },
    };

    // MOCK: annual summary rows
    m_annualRows = {
        QVariantMap{ { "year", "2024" }, { "income", "37 900 €" }, { "expenses", "29 400 €" }, { "saved", "8 500 €" },  { "rate", "22.4 %" } },
        QVariantMap{ { "year", "2025" }, { "income", "39 600 €" }, { "expenses", "29 900 €" }, { "saved", "9 700 €" },  { "rate", "24.5 %" } },
        QVariantMap{ { "year", "2026" }, { "income", "20 280 €" }, { "expenses", "14 850 €" }, { "saved", "5 430 €" },  { "rate", "26.8 %" } },
    };
}

void DataEntryController::saveSnapshot()
{
    // MOCK: doesn't persist
    setStatus(QStringLiteral("Mock — snapshot not persisted yet (UI pass)"));
}

void DataEntryController::addTransaction()
{
    // MOCK: doesn't persist
    setStatus(QStringLiteral("Mock — transaction not persisted yet (UI pass)"));
}

void DataEntryController::setStatus(const QString& msg)
{
    m_statusText = msg;
    emit statusChanged();
    QTimer::singleShot(3000, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}
