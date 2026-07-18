# Shared Components

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

`qml/components/` holds a shared UI library so pages don't hand-roll their own copy of the
same page-header, add/edit-dialog, confirm-dialog, badge, and button patterns. All of it lives
in the same `FinanceDashboard` QML module as every page, so no `import` line is needed — just
reference the type name directly.

- **`PageHeader.qml`** — `RowLayout`: optional back arrow (`showBack`), `title`, optional
  bare-red `"[MOCK]"` marker (`mock: true`), a stretch, then any caller-supplied trailing
  content (add buttons, `StatusBadge`s, filters) via the default property (`content`, same slot
  convention as `ChartCard.content`). `showBack: true` on pages reached by drilling into a
  sidebar action; left `false` on top-level category pages and the 5 mock analytics pages.
  `backClicked()` signal — caller decides what "back" means (usually `closeRequested()`).
- **`PillButton.qml`** — the big colored action button (Add/Save/Cancel/Delete in dialogs,
  "+ Add X" header buttons, Database Management's trigger buttons, Import's "Choose
  file"/"Save"/"Link this account"). `variant: "primary" | "neutral" | "danger"`; width is
  implicit (label + padding) unless the caller sets an explicit `width`/`height`.
  `fontSize`/`bold`/`horizontalPadding` are overridable for the handful of smaller/denser
  instances.
- **`GhostButton.qml`** — small bordered inline-row action ("✎ Edit", "＋ Subcategory", "＋
  Add hash", "Transactions", and the "×" remove square). `variant: "neutral"` keeps the
  caller's `textColor` at rest and adds a grey hover tint; `variant: "danger"` mutes the text to
  grey at rest and flashes red on hover. Every inline row action should use this — see
  `TODO.md` for a couple of chip removers that still don't.
- **`StatusBadge.qml`** — colored pill for every status/state indicator: parse-success/
  failure pills, the linked/not-linked badge, parse issue-count and log-level badges, the
  project on-track/gap badge. `variant: "success" | "warning" | "danger" | "neutral"`;
  `pillHeight` stays caller-set (26px status pills, 22-24px inline chips, 18px log-entry
  badges) with `radius` always half the height so it's always a true pill.
- **`FormRow.qml`** — one "label + TextField" row, styled identically everywhere a dialog
  needs free-text input (`bg #11111b`, `border #313244` / focus `#89b4fa`).
- **`ColorPickerRow.qml`** — 32×32 circle swatch + "Choose color…" pill + hex text, owns its
  own native `ColorDialog` instance internally (verified against
  doc.qt.io/qt-6/qml-qtquick-dialogs-colordialog.html). **Deliberately not a shared/singleton
  `ColorDialog`** — each dialog needs an independent in-progress color, and a declarative
  binding to a shared instance breaks permanently the first time the user picks a color — a
  fresh `ColorPickerRow` per usage sidesteps that entirely.
- **`FormDialog.qml`** — shared shell for every add/edit `Popup` (categories, subcategories,
  accounts, account types, account categories, add-hash, new-project). Caller supplies
  `dialogTitle`, `saveText`, and form rows via the default property; `saveClicked()` fires
  instead of auto-closing, so the caller can validate first and decide whether to actually
  close.
- **`ConfirmDialog.qml`** — shared shell for every destructive-action confirmation (delete
  category, delete account, delete account type/category, reset database, reset to mock data).
  `confirmVariant` defaults to `"danger"` but stays overridable per call.
- **`EmptyState.qml`** — "No … yet" placeholder text shown under a list's header when its
  model is empty.
- **`ColumnHeaderCell.qml`** — a table header label plus a drag-to-resize handle, used by
  `ImportSnapshotPage.qml`'s transaction table header (see `docs/statement-import.md`). The
  handle (a 2px grip inside a 6px hit target) is overlaid on the cell's right edge via a
  negative `anchors.rightMargin`, not inserted as a real `RowLayout` sibling — the header row's
  total preferred width never changes because of it. The component only *proposes* a new width
  via `widthChangeRequested(real)`; the caller's `columnWidths` object is the single source of
  truth both the header and every data-row cell read from.

**Not extracted, and why:** the account/category badge (arbitrary per-category color, not one
of the 4 `StatusBadge` variants), the hash/checkpoint chips on `AccountsManagePage.qml`, and
the per-page data tables (`ImportSnapshotPage.qml`'s transaction review table,
`AccountTransactionsPage.qml`'s read-only table, `ProjectsPage.qml`'s loan-scenario table) —
each has different columns/widths, so a generic `DataTable` would need more configuration
surface than it would save. `ProjectsPage.qml`'s hoverable project-card list also stays bespoke
— it reimplements `ChartCard`'s look but needs click/hover affordance `ChartCard` doesn't have
(a `hoverable` option on `ChartCard` itself is a candidate if a second page needs the same
pattern — tracked in `TODO.md`). The `descriptionPopup` (🔍 icon + selectable-text `Popup`) is
duplicated between `ImportSnapshotPage.qml` and `AccountTransactionsPage.qml` rather than
extracted — worth promoting to `qml/components/` if a third transaction table ever needs it.
