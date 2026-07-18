# Sidebar Navigation

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

`Main.qml` uses a left sidebar (`qml/components/SidebarCategory.qml`, repeated via a
`Repeater` over `Main.qml`'s `categories` array). The 8 entries mirror the first 5 `PROJECT.md`
pages 1:1, plus **Transactions**, **Account Management**, and **Database Management** — the
former single "Data Entry" page split into three lighter categories so account management,
hash management, and schema-reset tooling are each reachable independently.

**"ADMIN" section divider:** the sidebar `Repeater`'s delegate optionally renders a thin
divider + small "ADMIN" label above any category that declares `sectionLabel` in `Main.qml`'s
`categories` array — currently just **Account Management** (Database Management inherits the
visual grouping by sitting right after it). This distinguishes destructive/CRUD admin
operations on the app's own data model from the day-to-day analysis/data-entry categories
above it. Add `sectionLabel: "..."` to any future category object to start a new visual
section.

## Single source of truth: `currentViewId`

`root.currentViewId: string` is the **one** property that says what's on screen. Every
category and every wired action has a matching id (e.g. `"overview"`,
`"transactionsManagement"`, `"importTransactions"`, `"manageAccounts"`):

- **Category header** (label) — click sets `root.currentViewId = category.id`. Highlighted
  with a left accent bar (`#89b4fa`) + bold text whenever `currentViewId` is the category's own
  id, one of its `actions[].id`, or one of its `relatedViewIds` — computed once by `Main.qml`'s
  `isCategoryActive(category)` and handed down as `SidebarCategory.current`.
- **Arrow toggle** — a separate `MouseArea` (28px, right side of the header row) expands/
  collapses the category's action list *without* changing `currentViewId`.
  `SidebarCategory.expanded` auto-flips true the moment the category becomes active
  (`onCurrentChanged`), so navigating in from elsewhere always reveals which action put you
  there.
- **Action list** — `{ id, label }` objects (`categories[i].actions` in `Main.qml`). Clicking
  an action emits `SidebarCategory.actionTriggered(actionId)`; the row for the *currently
  active* action is highlighted the same way the category header is. Wired actions today:
  Overview → `"accountDetails"` (see `docs/account-history.md`); Transactions →
  `"importTransactions"` (see `docs/statement-import.md`), `"manageCategories"` (see
  `docs/category-management.md`), and `"accountTransactions"` (see
  `docs/account-management.md`); Account Management → `"manageAccounts"` and `"accountTypes"`
  (see `docs/account-management.md`). Database Management has no actions — its landing page
  holds the real controls directly (see `docs/database-management.md`). Every other action on
  the first 5 categories is still a no-op placeholder — `Main.qml`'s `onActionTriggered` only
  sets `currentViewId` when the id is in `root.overlayViewIds`.

**`accountTransactions` has two entry points** — the Transactions category's own sidebar
action (no preset account) *and* `AccountsManagePage.qml`'s per-row "Transactions" button
(preset account) — so a single `closeRequested()` target isn't enough;
`root.accountTransactionsReturnViewId` (`"transactionsManagement"` or `"manageAccounts"`) is
set right alongside `root.accountTransactionsIndex` at each entry point and read back by the
overlay's `onCloseRequested`.

To add a new sidebar category: append `{ id, label, actions }` to `Main.qml`'s `categories`
array, and add the matching page as the next child of the `StackLayout` (order must still match
array order).

**Overlay pages** (full-content pages reached from an action, not a category — e.g.
`ImportSnapshotPage`, `CategoriesManagePage`, `AccountsManagePage`,
`AccountTypesCategoriesPage`, `AccountTransactionsPage`, `AccountHistoryPage`): `Main.qml` uses
one `Loader` per overlay, anchored over the same content area as the `StackLayout`, each gated
by `active: root.currentViewId === "someId"`. `root.overlayViewIds` lists every id that means
"show an overlay, not the StackLayout"; `StackLayout.visible: !root.isOverlayView` reads that
one list. Each overlay page emits a `closeRequested()` signal that sets `root.currentViewId`
back to wherever "close" should land — this is the pattern to copy for the next wired-up
action.

`AccountTransactionsPage` is reached from *inside* `AccountsManagePage` (its "Transactions"
button, not a sidebar action) via `AccountsManagePage.viewTransactionsRequested(accountIndex)`
— `Main.qml` stores the index in `root.accountTransactionsIndex` and sets
`root.currentViewId = "accountTransactions"`. Its own `closeRequested()` sets `currentViewId`
back to `"manageAccounts"` when reached that way — the back arrow returns to where the user
came from. Because `"accountTransactions"` is never one of any category's `actions[].id` when
reached this way, the **Account Management** category declares it in a
`relatedViewIds: ["accountTransactions"]` array instead — `isCategoryActive()` checks that too,
so the sidebar still highlights "Account Management" while this page is open.

Every navigation entry point (category click, action click, "Transactions" button, every
overlay's `closeRequested()`) does exactly one thing — assign `root.currentViewId` — instead of
juggling multiple independent flags.

## QML delegate gotcha (general Qt Quick behavior, not specific to this app)

When a `TableView`/`Repeater` delegate declares ANY `required property`, model roles are **no
longer injected as context properties** — every role used (`display`, etc.) must then be
declared as a `required property` too. `TableView.editDelegate` is a separate component and can
still use context roles (matches the Qt docs example).
