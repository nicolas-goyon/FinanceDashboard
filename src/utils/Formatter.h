#pragma once

#include <QString>
#include <cmath>

// ── Fmt — display formatting utilities ───────────────────────────────────────
//
// Usage:
//   Fmt::gCurrency = Fmt::Currency::EUR;   // or USD — set once at startup
//   QString s = Fmt::money(12345.6);       // "12 345,60 €"
//   QString s = Fmt::money(-500.0, true);  // "-500,00 €"
//   QString s = Fmt::money(80.5,   true);  // "+80,50 €"

namespace Fmt {

enum class Currency { EUR, USD };

/// Global currency switch — change this one variable to re-label the whole app.
inline Currency gCurrency = Currency::EUR;

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
    int  cents   = static_cast<int>(std::round((absVal - static_cast<double>(intPart)) * 100.0));
    if (cents == 100) { ++intPart; cents = 0; }

    // Group integer part with narrow no-break spaces (U+202F) every 3 digits
    const QString intStr = QString::number(intPart);
    QString grouped;
    const int len = intStr.length();
    for (int i = 0; i < len; ++i) {
        if (i > 0 && (len - i) % 3 == 0)
            grouped += QChar(0x202F); // narrow no-break space
        grouped += intStr[i];
    }

    // Assemble amount:  "1 234,56"
    const QString amount = grouped
                         + QLatin1Char(',')
                         + QString::number(cents).rightJustified(2, QLatin1Char('0'));

    // Sign prefix
    QString sign;
    if (negative)       sign = QStringLiteral("-");
    else if (showPlus)  sign = QStringLiteral("+");

    // Currency symbol (after the amount, French convention)
    const QString symbol = (gCurrency == Currency::EUR)
                           ? QStringLiteral(" €")   // " €"
                           : QStringLiteral(" $");        // " $"

    return sign + amount + symbol;
}

} // namespace Fmt
