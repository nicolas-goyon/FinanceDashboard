#include "DataEntryController.h"

#include <QTimer>

DataEntryController::DataEntryController(QObject* parent)
    : QObject(parent)
{
    // MOCK: seed accounts, not persisted. "iban" / "bic" / "number" fields are
    // MOCK too — used by SnapshotImportController to demo auto-matching an
    // imported statement to one of these accounts via an IBAN+BIC hash (see
    // SnapshotImportController::accountHash() / § Statement Import in
    // ARCHITECTURE.MD). "bic" is only meaningful where "iban" is set.
    m_accounts = {
        QVariantMap{ { "name", "Checking — Bank A" },  { "type", "Checking" }, { "iban", "FR7630001007941234567890185" }, { "bic", "BNPAFRPPXXX" }, { "number", "00012345678" } },
        QVariantMap{ { "name", "Checking — Bank B" },  { "type", "Checking" }, { "iban", "FR7612345987651234567890147" }, { "bic", "SOGEFRPPXXX" }, { "number", "00087654321" } },
        QVariantMap{ { "name", "Livret A" },           { "type", "Savings"  }, { "iban", "" }, { "bic", "" }, { "number", "" } },
        QVariantMap{ { "name", "LDDS" },               { "type", "Savings"  }, { "iban", "" }, { "bic", "" }, { "number", "" } },
        QVariantMap{ { "name", "Assurance Vie" },      { "type", "Savings"  }, { "iban", "" }, { "bic", "" }, { "number", "" } },
        QVariantMap{ { "name", "Trading account" },    { "type", "Trading"  }, { "iban", "" }, { "bic", "" }, { "number", "" } },
    };

    m_accountTypes = { "Checking", "Savings", "Trading" };

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

void DataEntryController::addAccount(const QString& name, const QString& type)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || !m_accountTypes.contains(type))
        return;

    for (const QVariant& v : m_accounts)
        if (v.toMap()["name"].toString().compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(QStringLiteral("⚠  An account with this name already exists"));
            return;
        }

    m_accounts.append(QVariantMap{ { "name", trimmed }, { "type", type }, { "iban", "" }, { "bic", "" }, { "number", "" } });
    emit accountsChanged();
    setStatus(QStringLiteral("✓  Account added (mock — not persisted yet)"));
}

void DataEntryController::removeAccount(int index)
{
    if (index < 0 || index >= m_accounts.size())
        return;
    m_accounts.removeAt(index);
    emit accountsChanged();
    setStatus(QStringLiteral("✓  Account removed (mock — not persisted yet)"));
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
