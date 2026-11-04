#ifndef FINANCEDASHBOARD_UTILS_ACCOUNTHASHING_H
#define FINANCEDASHBOARD_UTILS_ACCOUNTHASHING_H

#include <QByteArray>
#include <QChar>
#include <QCryptographicHash>
#include <QLatin1Char>
#include <QLatin1String>
#include <QMessageAuthenticationCode>
#include <QString>
#include <QtTypes>

#include <algorithm>

// ── AccountHashing — the one-byte tag of a bank account ─────────────────────
//
// What it is for: the import suggests the account of the app that the
// statements of a bank account went to, without storing anything that gives
// the IBAN back (privacy rule, maintainer 2026-10-29; option B of
// 2026-11-04, #203).
//
// The tag is ONE byte (two hexadecimal characters), the first byte of
// PBKDF2-HMAC-SHA256 of the normalized IBAN, with a fixed salt of the app and
// kIterations rounds. A French IBAN of a known bank has about 10^11
// candidates, so hundreds of millions of IBANs share each of the 256 tag
// values: a tag cannot give the IBAN back. It is enough to tell apart the
// few accounts of one user; when two accounts have the same tag, the user
// picks the account at the import. The slow derivation also makes a test of
// a guessed IBAN cost 0.1 s or more.
//
// The same IBAN gives the same tag on every install (fixed salt). The BIC is
// not used. Until v0.3.0 the app stored an HMAC-SHA256 of the IBAN keyed by
// the BIC (64 characters): a person with the database could find the IBAN
// by brute force. Schema step 7 removed those (Database::migrateTo7()).
//
// The IBAN of a statement stays in memory during its review. compute() is
// empty when there is no IBAN (nothing to match on).

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
// The first byte of PBKDF2-HMAC-SHA256(password, salt, iterations) (RFC 8018,
// one block: dkLen <= 32).
inline quint8 pbkdf2FirstByte(const QByteArray& password, const QByteArray& salt, int iterations)
{
    const QByteArray blockIndex("\x00\x00\x00\x01", 4);
    QByteArray u = QMessageAuthenticationCode::hash(salt + blockIndex, password, QCryptographicHash::Sha256);
    QByteArray t = u;
    for (int i = 1; i < iterations; ++i) {
        u = QMessageAuthenticationCode::hash(u, password, QCryptographicHash::Sha256);
        for (qsizetype j = 0; j < t.size(); ++j) {
            t[j] = static_cast<char>(t.at(j) ^ u.at(j));
        }
    }
    return static_cast<quint8>(t.at(0));
}
} // namespace detail

inline constexpr int kIterations = 100000;
inline constexpr QLatin1String kSalt("FinanceDashboard account tag v1");

// The tag of the account of a statement: two lower-case hexadecimal
// characters, or "" when there is no IBAN. `bic` is not used (kept for the
// callers).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — every call site
// passes the iban and the bic of one statement, with these names.
inline QString compute(const QString& iban, const QString& bic)
{
    Q_UNUSED(bic)
    const QString ibanNorm = normalize(iban);
    if (ibanNorm.isEmpty()) {
        return {};
    }
    const quint8 tag = detail::pbkdf2FirstByte(ibanNorm.toUtf8(), QByteArray(kSalt.data(), kSalt.size()), kIterations);
    return QStringLiteral("%1").arg(tag, 2, 16, QLatin1Char('0'));
}

// The fingerprint of before #50: not used any more (step 7 removed every
// stored fingerprint). Always empty.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — as compute().
inline QString legacyCompute(const QString& iban, const QString& bic)
{
    Q_UNUSED(iban)
    Q_UNUSED(bic)
    return {};
}

// The form in which the app stores and compares a fingerprint (#47): no
// spaces around it, in lower case, as compute() gives it. SQLite compares
// text with its case, so every write and every lookup uses this form.
inline QString storedForm(const QString& hash)
{
    return hash.trimmed().toLower();
}

// True when `tag` (in its stored form) is a tag as compute() gives it: 2
// lower-case hexadecimal characters.
inline bool isTag(const QString& tag)
{
    constexpr qsizetype kTagLength = 2;
    return tag.size() == kTagLength && std::all_of(tag.cbegin(), tag.cend(), [](QChar character) {
               return (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
                      || (character >= QLatin1Char('a') && character <= QLatin1Char('f'));
           });
}

// True when `hash` (in its stored form) is a fingerprint of before v0.3.0:
// 64 hexadecimal characters. For schema step 5 only.
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
