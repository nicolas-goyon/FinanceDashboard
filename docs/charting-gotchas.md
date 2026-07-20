# Charting Gotchas (learned on this project)

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

Always verify a Qt Graphs property against
[QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples) and
https://doc.qt.io/qt-6/qtgraphs-index.html before using it — never invent a property name or
value. The notes below are pitfalls already hit and fixed on this project; they don't replace
checking the docs for anything new.

- **Axis labels cannot render "€"** — `ValueAxis.labelFormat` shows `?` for non-ASCII. Units go
  in the `ChartCard` title; tooltips are QML `Text` and render € fine.
- **Hover support**: `LineSeries` → `pointDelegate` (`qml/components/HoverPoint.qml`, uses
  injected `pointValueX/Y`, `pointIndex`); `BarSeries` → `barDelegate` (`HoverBar.qml`, injected
  `barValue`/`barLabel`) — `BarSeries` has NO hover signals. `PieSeries` has **no hover at
  all**; only `clicked(slice)` → pages show slice details on click.
- **`HoverPoint.qml`'s hit-box is 34×34, bigger than its 8–13px visible dot** — a small hit-box
  the same size as the dot was hard to actually land the cursor on (product feedback on
  `AccountHistoryPage.qml`'s balance chart). The `HoverHandler` reacts anywhere in the larger
  `Item`, not just over the drawn marker; applies to every page using `HoverPoint` (it's shared).
- **`HoverPoint.qml`'s tooltip flips below the point and clamps horizontally instead of always
  centering above it** — always anchoring above+centered (the original layout) got the tooltip
  clipped for any point near the chart's top edge or a side (product feedback — a real spike
  near the top of `AccountHistoryPage.qml`'s balance chart). It now computes `x`/`y` directly:
  `showBelow` (goes below instead of above when there isn't `tooltip.height` of room above,
  using `root.y`) and a horizontal clamp against `root.parent`'s width (Qt Graphs' own
  delegate-host item — falls back to plain centering if that item isn't a sensibly-sized one, so
  this can't regress into something worse than the original centering if the assumption about
  Qt Graphs' internal parent item is ever wrong). `z: 1000` keeps it above sibling delegates, but
  note z doesn't escape an *ancestor's* `clip: true` — the position clamp is what actually fixes
  the cropping, not the z bump.
- **Any new chart should have hover (or, for `PieSeries`, click) feedback by default** — every
  existing chart in this app already shows a tooltip/detail line on hover or click (see the point
  above for which mechanism each series type supports), so a new chart with no way to read exact
  values on interaction is the exception, not the norm. Don't add a chart with none of these
  without a specific reason.
- **Confirmed against Qt's own C++ class docs, not just the QML pages** (`qpieseries.html`,
  `qpieslice.html`): `QPieSeries`'s full signal list is `added`/`removed`/`clicked`/
  `doubleClicked`/`pressed`/`released`/the property-changed signals — genuinely **no hover
  signal exists anywhere on `PieSeries`/`PieSlice`**, in either the QML or C++ API. (One prose
  sentence on `QPieSlice`'s C++ page claims slices "emit signals... when users... hover the
  mouse over them" — that line doesn't match either class's actual signal list and looks like
  boilerplate copied from another series type's doc template; don't trust it over the enumerated
  signals.) A hover tooltip on a pie would require hand-building a mouse-position → slice
  hit-test overlay (angle/radius math Qt doesn't expose) — treated as not worth the fragility on
  this project; every pie here stays click-only.
- **`AreaSeries` has no `pointDelegate`** — stacked areas get transparent `LineSeries` "hover
  carriers" on each boundary; tooltip shows the layer's own (non-cumulative) value via
  `HoverPoint.overrideValues`.
- **Child `Component.onCompleted` runs before the parent's** — series filled by a page-level
  handler must be appended there, not in the child.
- **Sankey**: no Qt Graphs series type exists for it — `qml/components/MoneyFlow.qml` is a
  pure-QML Canvas port of QtGraphsExamples → flowchart.
- **Inline `component` declarations** must sit at the document root, not nested in a `Popup`.
- Donuts use `pieSize: 0.75` / `holeSize: 0.42` (`0.85` clipped the labels).
- **A "2-level"/nested/"sunburst" pie: `PieSlice` sub-slices exist, but Qt Graphs never
  documents which ring (inner or outer) they render as relative to their parent — use two
  independent `PieSeries` instead when the ring order matters.** `PieSlice` can contain sub
  slices (`slice.append(label, value)` on the slice itself, not just the series — see
  `qml-qtgraphs-pieslice.html`, "A pie slice can contain sub slices"), and `PieSlice` also has a
  `subSlicesRatio` property (their size *relative to* the parent slice, default `0.6`) — but
  neither the QML nor the C++ (`qpieslice.html`) docs say whether the parent slice's own ring
  ends up drawn inside or outside its sub-slices' ring. Empirically (product feedback on
  `AccountHistoryPage.qml`'s first version, which used native sub-slices): the **parent slice
  renders as the outer ring**, sub-slices as the inner one — the opposite of what a typical
  "drill down = go outward" sunburst reads as, and not something to assume without checking.
  When a page needs the *other* order (this page: "subcategories must be the outer ring"),
  don't gamble on the unspecified sub-slice convention — use **two separate `PieSeries`, each
  in its *own* `GraphsView`, stacked via `anchors.fill`**, each with its own explicit
  `pieSize`/`holeSize` (categories: `pieSize: 0.55`/`holeSize: 0.28`; subcategories:
  `pieSize: 0.92`/`holeSize: 0.58`, starting right where the category ring ends). Both series
  iterate the same category list in the same order and a category's own value always equals the
  sum of its subcategories' values (by construction — see
  `AccountController::accountCategoryBreakdown()`), so the two rings' slice boundaries land on
  the same angles with no manual angle math — same visual result as native sub-slices, but with
  a guaranteed inner/outer assignment.
  - **Two `PieSeries` sharing *one* `GraphsView` is worth avoiding, not just for ring order —
    `onClicked` stopped firing on either series when tried that way** (both rings went
    unclickable on this page's first version of this feature). Every other pie in this app
    (Import report, Cash Flow) is one `PieSeries` per `GraphsView` — the only configuration Qt
    Graphs actually demonstrates in its own examples — so that's what each ring now gets:
    its own `GraphsView`, anchored exactly on top of the other's, rather than two series packed
    into one view. Root cause unconfirmed (Qt Graphs doesn't document multi-series-per-view
    interaction either way), but isolating each series into its own view matches the only
    proven-working pattern and fixed it.
  - **Subcategory (or any densely-sliced) ring needs `labelVisible = true` set per sub-slice
    explicitly** — it does *not* inherit visibility from the parent ring's slices. Pair with
    `PieSeries.angleSpanVisibleLimit`/`angleSpanLabelVisibility` (Qt 6.10+, verified on
    doc.qt.io before using) to auto-hide labels on the thinnest slices instead of letting many
    overlapping label arms pile up.
- **`DateTimeAxis` with `min === max` reproducibly crashed the app** (native crash, no QML
  console error at all) — hit on `AccountHistoryPage.qml`'s balance-over-time chart for any
  account whose transactions all fall on a single calendar day (a very ordinary case — one
  transaction, or several all dated the same day). Qt Graphs' own docs don't forbid an equal
  min/max, but automatic tick-layout almost certainly divides by the axis range internally, so
  a zero-width range blows up in native code — bypassing QML's warning/error console entirely.
  **Fix: always pad a degenerate axis range** (see `AccountHistoryPage.qml`'s `maxTimeMs`, which
  pads by one day whenever the raw max wouldn't exceed the raw min). Applies to any future
  `DateTimeAxis`/`ValueAxis` whose min/max are computed from real (not hand-picked MOCK) data —
  `SavingsPage.qml`/`CashFlowPage.qml`'s existing charts never hit this because their MOCK data
  always spans many distinct months by construction, not because the pattern itself was safe.
