#ifndef FINANCEDASHBOARD_UTILS_ACCOUNTHASHING_H
#define FINANCEDASHBOARD_UTILS_ACCOUNTHASHING_H

#include <QChar>
#include <QCryptographicHash>
#include <QLatin1Char>
#include <QLatin1String>
#include <QMessageAuthenticationCode>
#include <QString>
#include <QtTypes>

#include <algorithm>

// ── AccountHashing — the fingerprint of a bank account ──────────────────────
//
// What it is for (maintainer, 2026-10-17, #50): the fingerprint groups the
// statements of one bank account, and the import suggests the account of
// the app that has it, without storing the IBAN or the BIC. The app keeps the
// fingerprint only (account_hashes, see AccountHashesTable). The IBAN and the
// BIC of a statement stay in memory during its review.
//
// What it is not: a security feature. The fingerprint is an HMAC-SHA256 of the
// IBAN, keyed by the BIC. A BIC is public, and for a known bank and branch a
// French IBAN has about 10^11 candidates (its check digits come from the other
// digits). So a person who gets the database can find the IBAN of a
// fingerprint by brute force, in minutes on a GPU. That is acceptable for the
// grouping and the suggestion; it matters only if the database leaks. A key
// outside the database (a secret for each install) would make it stronger, but
// then the fingerprints of two installs would not match: not done.
//
// The IBAN and the BIC are normalized first: no spaces, upper case, and the
// BIC on 11 characters. An 8-character BIC is the primary office of the bank,
// whose branch code is "XXX" (ISO 9362), so PSSTFRPP and PSSTFRPPXXX give one
// fingerprint (#50). Before #50 (v0.2.1, and the pre-releases of v0.2.2 before
// it), the key was the normalized BIC without "XXX": the import also tries
// legacyCompute() (the 8-character form), and the save replaces that
// fingerprint with compute().
//
// Every caller uses these functions: a fingerprint computed another way would
// not match. compute() is empty when there is no IBAN (nothing to match on).

namespace AccountHashing {

inline QString normalize(QString value)
{
    value = value.trimmed().toUpper();
    value.remove(QLatin1Char(' '));
    return value;
}

// The normalized BIC on 11 characters: an 8-character BIC gets the branch
// code "XXX". A BIC of another length stays as it is.
inline QString normalizeBic(const QString& bic)
{
    constexpr qsizetype kPrimaryOfficeLength = 8;
    QString value = normalize(bic);
    if (value.size() == kPrimaryOfficeLength) {
        value += QLatin1String("XXX");
    }
    return value;
}

namespace detail {
// HMAC-SHA256 of `iban`, keyed by `key`, in lower-case hexadecimal.
inline QString hmac(const QString& iban, const QString& key)
{
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
    mac.setKey(key.toUtf8());
    mac.addData(iban.toUtf8());
    return QString::fromLatin1(mac.result().toHex());
}
} // namespace detail

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — every call site
// passes the iban and the bic of one statement, with these names.
inline QString compute(const QString& iban, const QString& bic)
{
    const QString ibanNorm = normalize(iban);
    if (ibanNorm.isEmpty()) {
        return {};
    }
    return detail::hmac(ibanNorm, normalizeBic(bic));
}

// The fingerprint that the app stored before #50 for a statement with the
// 8-character BIC of a primary office: the key was that normalized BIC,
// without "XXX". Empty when compute() is the only form: no IBAN, or a BIC of another
// branch (its 11 characters are the key in both versions).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — as compute().
inline QString legacyCompute(const QString& iban, const QString& bic)
{
    constexpr qsizetype kFullLength = 11;
    constexpr qsizetype kPrimaryOfficeLength = 8;
    const QString ibanNorm = normalize(iban);
    const QString full = normalizeBic(bic);
    if (ibanNorm.isEmpty() || full.size() != kFullLength || !full.endsWith(QLatin1String("XXX"))) {
        return {};
    }
    return detail::hmac(ibanNorm, full.left(kPrimaryOfficeLength));
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
