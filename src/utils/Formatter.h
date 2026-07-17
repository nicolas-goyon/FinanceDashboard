#ifndef FINANCEDASHBOARD_UTILS_FORMATTER_H
#define FINANCEDASHBOARD_UTILS_FORMATTER_H

#include <QDate>
#include <QString>
#include <cmath>
#include <cstdint>

// ── Fmt — display formatting utilities ───────────────────────────────────────
//
// Usage:
//   Fmt::currency() = Fmt::Currency::EUR;  // or USD — set once at startup
//   QString s = Fmt::money(12345.6);       // "12 345,60 €"
//   QString s = Fmt::money(-500.0, true);  // "-500,00 €"
//   QString s = Fmt::money(80.5,   true);  // "+80,50 €"
//   QString s = Fmt::dateOrDash(someDate); // "16/07/2026" or "—" if invalid

namespace Fmt {

namespace detail {
constexpr int kCentsPerUnit = 100;              // 100 cents per euro/dollar
constexpr char16_t kNarrowNoBreakSpace = 0x202F; // U+202F thousands separator
} // namespace detail

enum class Currency : std::uint8_t { EUR, USD };

/// Global currency switch — change this one value to re-label the whole app.
/// Wrapped in a function (function-local static) rather than a raw global
/// variable so there is no mutable global at namespace scope
/// (cppcoreguidelines-avoid-non-const-global-variables); the mutable state
/// itself is unchanged, still "set once at startup" via `Fmt::currency() = ...`.
inline Currency& currency()
{
    static Currency instance = Currency::EUR;
    return instance;
}

/// Format a monetary value using French locale conventions:
///   - Thousands separator : narrow no-break space (U+202F)
///   - Decimal separator   : comma
///   - Currency symbol     : placed after the amount with a space
///       EUR → "1 234,56 €"
///       USD → "1 234,56 $"
///
/// @param value    Amount to format (negative → minus sign prepended).
/// @param showPlus Prepend '+' for zero / positive values when true.
inline QString money(double value, bool showPlus = false)
{
    const bool negative = (value < 0.0);
    const double absVal = std::abs(value);

    // Split into integer and cents, handling floating-point rounding
    auto intPart = static_cast<long long>(absVal);
    int  cents   = static_cast<int>(std::round((absVal - static_cast<double>(intPart))
                                                * detail::kCentsPerUnit));
    if (cents == detail::kCentsPerUnit) { ++intPart; cents = 0; }

    // Group integer part with narrow no-break spaces (U+202F) every 3 digits
    const QString intStr = QString::number(intPart);
    QString grouped;
    const int len = static_cast<int>(intStr.length());
    for (int i = 0; i < len; ++i) {
        if (i > 0 && (len - i) % 3 == 0) {
            grouped += QChar(detail::kNarrowNoBreakSpace);
        }
        grouped += intStr.at(i);
    }

    // Assemble amount:  "1 234,56"
    const QString amount = grouped
                         + QLatin1Char(',')
                         + QString::number(cents).rightJustified(2, QLatin1Char('0'));

    // Sign prefix
    QString sign;
    if (negative)      { sign = QStringLiteral("-"); }
    else if (showPlus) { sign = QStringLiteral("+"); }

    // Currency symbol (after the amount, French convention)
    const QString symbol = (currency() == Currency::EUR)
                           ? QStringLiteral(" €")   // " €"
                           : QStringLiteral(" $");        // " $"

    return sign + amount + symbol;
}

/// Display-format a date as "dd/MM/yyyy", or "—" when invalid/unset.
/// Hoisted here from two byte-for-byte-identical anonymous-namespace
/// dateText() helpers (SnapshotImportController.cpp, AccountController.cpp)
/// — see ARCHITECTURE.MD § Shared Components.
inline QString dateOrDash(const QDate& date)
{
    return date.isValid() ? date.toString(QStringLiteral("dd/MM/yyyy")) : QStringLiteral("—");
}

} // namespace Fmt

#endif // FINANCEDASHBOARD_UTILS_FORMATTER_H
