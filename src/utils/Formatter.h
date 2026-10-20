#ifndef FINANCEDASHBOARD_UTILS_FORMATTER_H
#define FINANCEDASHBOARD_UTILS_FORMATTER_H

#include <QChar>
#include <QDate>
#include <QLatin1String>
#include <QString>
#include <Qt>
#include <cmath>
#include <cstdint>

// ── Fmt — display formatting utilities ───────────────────────────────────────
//
// Usage:
//   Fmt::currency() = Fmt::Currency::EUR;  // or USD — set once at startup
//   QString s = Fmt::money(12345.6);       // "12 345,60 €"
//   QString s = Fmt::money(-500.0, true);  // "-500,00 €"
//   QString s = Fmt::money(80.5,   true);  // "+80,50 €"
//   QString s = Fmt::money(80.5, Fmt::currencyFromCode(account.currency)); // "80,50 $"
//   QString s = Fmt::dateOrDash(someDate); // "16/07/2026" or "—" if invalid

namespace Fmt {

namespace detail {
constexpr int kCentsPerUnit = 100;              // 100 cents per euro/dollar
constexpr char16_t kNarrowNoBreakSpace = 0x202F; // U+202F thousands separator
// Larger amounts are not formatted: the cents would not fit a long long, and
// no amount of this app comes near it.
constexpr double kMaxAmount = 1e15;
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

/// The currency of an account, from its `currency` column ("EUR" or "USD",
/// see AccountsManagePage.qml's currency selector). Anything else is EUR.
inline Currency currencyFromCode(const QString& code)
{
    return code.compare(QLatin1String("USD"), Qt::CaseInsensitive) == 0 ? Currency::USD : Currency::EUR;
}

/// "€" or "$", for a label that names the unit (a chart axis, say).
inline QString currencySymbol(Currency unit)
{
    return unit == Currency::USD ? QStringLiteral("$") : QStringLiteral("€");
}

/// Format a monetary value using French locale conventions:
///   - Thousands separator : narrow no-break space (U+202F)
///   - Decimal separator   : comma
///   - Currency symbol     : placed after the amount with a space
///       EUR → "1 234,56 €"
///       USD → "1 234,56 $"
///
/// The value is rounded to cents first, and the sign comes from the rounded
/// value: -0.001 is "0,00 €", not "-0,00 €". NaN, an infinity and an amount
/// above 10^15 give "—" (#48).
///
/// @param value    Amount to format (negative → minus sign prepended).
/// @param unit     The currency of the amount (the account's, for an amount
///                 of one account).
/// @param showPlus Prepend '+' for zero / positive values when true.
inline QString money(double value, Currency unit, bool showPlus = false)
{
    if (!std::isfinite(value) || std::abs(value) > detail::kMaxAmount) {
        return QStringLiteral("—");
    }
    const long long totalCents = std::llround(value * detail::kCentsPerUnit);
    const bool negative = totalCents < 0;
    const long long absCents = negative ? -totalCents : totalCents;
    const long long intPart = absCents / detail::kCentsPerUnit;
    const auto cents = static_cast<int>(absCents % detail::kCentsPerUnit);

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

    // Currency symbol (after the amount, French convention), after a narrow
    // no-break space (U+202F)
    return sign + amount + QChar(detail::kNarrowNoBreakSpace) + currencySymbol(unit);
}

/// money() in the currency of the app (currency()).
inline QString money(double value, bool showPlus = false)
{
    return money(value, currency(), showPlus);
}

/// Display-format a date as "dd/MM/yyyy", or "—" when invalid/unset.
/// Hoisted here from two byte-for-byte-identical anonymous-namespace
/// dateText() helpers (SnapshotImportController.cpp, AccountController.cpp)
/// — see ARCHITECTURE.MD § Shared Components.
inline QString dateOrDash(const QDate& date)
{
    return date.isValid() ? date.toString(QStringLiteral("dd/MM/yyyy")) : QStringLiteral("—");
}

namespace detail {
// Buckets for the import review screen's CONFIDENCE badge. Chosen to line up
// with what the category predictor actually produces (see
// src/predict/CategoryPredictor.h): a merchant seen once scores 0.67 → amber
// "check this", seen twice scores 0.80 → green.
constexpr double kConfidenceDangerBelow  = 0.5;
constexpr double kConfidenceWarningBelow = 0.8;
constexpr double kPercentScale = 100.0;
} // namespace detail

/// StatusBadge variant name for a 0.0–1.0 prediction confidence.
/// `confidence <= 0` is **"not scored yet"**, not "the model is 0% sure" —
/// it renders neutral, never as a failure.
/// Hoisted here from ImportSnapshotPage.qml's page-level `confidenceVariant()`
/// JS helper so the thresholds live next to every other display rule instead
/// of being computed client-side — see TODO.md § Convention drift.
inline QString confidenceVariant(double confidence)
{
    if (confidence <= 0.0)                                { return QStringLiteral("neutral"); }
    if (confidence < detail::kConfidenceDangerBelow)      { return QStringLiteral("danger"); }
    if (confidence < detail::kConfidenceWarningBelow)     { return QStringLiteral("warning"); }
    return QStringLiteral("success");
}

/// Badge text for a 0.0–1.0 prediction confidence: "83 %", or "—" when the
/// row hasn't been scored. Counterpart to confidenceVariant() above.
inline QString confidenceText(double confidence)
{
    if (confidence <= 0.0) {
        return QStringLiteral("—");
    }
    return QString::number(std::lround(confidence * detail::kPercentScale)) + QStringLiteral(" %");
}

} // namespace Fmt

#endif // FINANCEDASHBOARD_UTILS_FORMATTER_H
