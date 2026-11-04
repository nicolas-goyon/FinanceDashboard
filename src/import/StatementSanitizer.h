#ifndef FINANCEDASHBOARD_IMPORT_STATEMENTSANITIZER_H
#define FINANCEDASHBOARD_IMPORT_STATEMENTSANITIZER_H

#include <QString>
#include <QStringList>

// ── StatementSanitizer ───────────────────────────────────────────────────────
//
// Privacy rule (maintainer, 2026-10-29, #202): a person with full access to
// the machine, the database, the files and the logs must not see or recover
// anything more than the user sees in the UI. No bank identifier is stored,
// in any form that can be reversed.
//
// Every import template passes the text of each row through clean() before
// it returns the row, so nothing downstream (the review, the database, the
// logs) ever gets an identifier. It removes:
//
//   - the reference lines of the bank ("Internal ref.: …", "External ref.: …"):
//     the whole line;
//   - IBANs, also with spaces;
//   - labelled ids: BIC, IBAN, RUM, ICS, mandate and creditor ids, "ref:";
//   - card numbers: "Card payment 1234", "CB 1234", masked forms
//     ("XXXX1234", "**** 1234");
//   - runs of 10 or more digits (account numbers, client codes);
//   - the identifiers of the statement header (holder name, IBAN, BIC,
//     account number, client code), wherever they appear.
//
// Merchant and counterparty names stay (maintainer decision of 2026-11-04).
// A removed value becomes kMask, so the user still sees that something was
// there ("Card payment •••: STORE").
//
// The bank reference is still needed to tell rows apart (the fingerprint of
// TransactionsTable): a template reads it from the raw text first, keeps it
// in memory only (Data::ImportedTransaction::bankReference), and the
// database stores only its SHA-256.
namespace StatementSanitizer {

inline constexpr QStringView kMask = u"\u2022\u2022\u2022"; // •••

// `text` without the identifiers above. `secrets` are values of the
// statement header to remove too (empty values are skipped). Single-line
// text in, single-line text out; the line structure of a multi-line text
// is kept.
[[nodiscard]] QString clean(const QString& text, const QStringList& secrets = {});

// The raw (multi-line) description without its reference lines, then
// cleaned and made single-line.
[[nodiscard]] QString cleanDescription(const QString& rawDescription, const QStringList& secrets = {});

// A description stored before #202: one line, with the reference block of
// the bank at its end ("… External ref.: 123 Internal ref.: a1b2…"). The
// block goes (labels with "ref." and a dot, as the bank writes them), then
// clean(). For the clean-up of the existing databases (#204).
[[nodiscard]] QString cleanStoredDescription(const QString& description);

// True when `text` still holds something that clean() removes: for tests.
[[nodiscard]] bool hasIdentifier(const QString& text, const QStringList& secrets = {});

} // namespace StatementSanitizer

#endif // FINANCEDASHBOARD_IMPORT_STATEMENTSANITIZER_H
