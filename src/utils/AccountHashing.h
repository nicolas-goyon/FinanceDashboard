#ifndef FINANCEDASHBOARD_UTILS_ACCOUNTHASHING_H
#define FINANCEDASHBOARD_UTILS_ACCOUNTHASHING_H

#include <QChar>
#include <QCryptographicHash>
#include <QLatin1Char>
#include <QMessageAuthenticationCode>
#include <QString>
#include <QtTypes>

#include <algorithm>

// ── AccountHashing ────────────────────────────────────────────────────────────
//
// Shared "fingerprint" used to identify a bank account without comparing raw
// IBAN/BIC values directly: HMAC-SHA256 of the (normalized) IBAN, keyed by the
// (normalized) BIC. Originally lived only in
// SnapshotImportController::accountHash() (mocking a lookup-by-fingerprint DB
// call — see ARCHITECTURE.MD § Statement Import); extracted here once
// AccountController needed the exact same computation to auto-derive a stored
// hash for an account with an IBAN+BIC on file (see § Account Management).
// Both call sites must use this one function — never re-derive the HMAC
// independently, or a real account and a real statement could compute
// different fingerprints for the same IBAN/BIC due to normalization drift.
//
// Returns an empty string when there's no IBAN to hash (nothing to match on).

namespace AccountHashing {

inline QString normalize(QString value)
{
    value = value.trimmed().toUpper();
    value.remove(QLatin1Char(' '));
    return value;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — both call sites
// (AccountController, SnapshotImportController) pass local variables already
// named iban/bic matching these params, so a swap isn't a realistic mistake.
inline QString compute(const QString& iban, const QString& bic)
{
    const QString ibanNorm = normalize(iban);
    if (ibanNorm.isEmpty()) {
        return {};
    }

    QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
    mac.setKey(normalize(bic).toUtf8());
    mac.addData(ibanNorm.toUtf8());
    return QString::fromLatin1(mac.result().toHex());
}

// The form in which the app stores and compares a fingerprint (#47): no
// spaces around it, in lower case, as compute() gives it. SQLite compares
// text with its case, so every write and every lookup uses this form.
inline QString storedForm(const QString& hash)
{
    return hash.trimmed().toLower();
}

// True when `hash` (in its stored form) is a fingerprint as compute() gives
// it: 64 hexadecimal characters. Only such a value can match a statement.
inline bool isFingerprint(const QString& hash)
{
    constexpr qsizetype kHexLength = 64; // SHA-256: 32 bytes
    return hash.size() == kHexLength && std::all_of(hash.cbegin(), hash.cend(), [](QChar character) {
               return (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
                      || (character >= QLatin1Char('a') && character <= QLatin1Char('f'));
           });
}

} // namespace AccountHashing

#endif // FINANCEDASHBOARD_UTILS_ACCOUNTHASHING_H
