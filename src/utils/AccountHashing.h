#pragma once

#include <QString>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

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

inline QString normalize(QString s)
{
    s = s.trimmed().toUpper();
    s.remove(QLatin1Char(' '));
    return s;
}

inline QString compute(const QString& iban, const QString& bic)
{
    const QString ibanNorm = normalize(iban);
    if (ibanNorm.isEmpty())
        return QString();

    QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
    mac.setKey(normalize(bic).toUtf8());
    mac.addData(ibanNorm.toUtf8());
    return QString::fromLatin1(mac.result().toHex());
}

} // namespace AccountHashing
