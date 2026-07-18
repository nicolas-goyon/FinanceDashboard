# Transactions

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

Own sidebar category — the successor to the old "Data Entry" category, once accounts and
database resets moved to their own categories, leaving just the transaction-import +
category-management actions here (plus, later, transaction viewing/editing — see below).
`qml/pages/TransactionsManagementPage.qml` is this category's own landing page: a minimal
placeholder (header + text pointing at the three sidebar actions below) — no controller, no
mock content, since all three actions already have their own real pages. File/type name kept
as `TransactionsManagementPage` — only the sidebar label/id differs from the class name, same
"label ≠ file name" precedent as `SnapshotImportController`.

Actions:

- **"Import Transactions"** — `ImportSnapshotPage.qml` / `SnapshotImportController`, see
  `docs/statement-import.md`.
- **"Categories & Subcategories"** — `CategoriesManagePage.qml` / `CategoryController`, see
  `docs/category-management.md`.
- **"View & Edit Transactions"** — `AccountTransactionsPage.qml` / `AccountController`, see
  `docs/account-management.md`. Also still reachable from Manage Accounts' per-row
  "Transactions" button — see that doc for the dual-entry-point mechanics.
