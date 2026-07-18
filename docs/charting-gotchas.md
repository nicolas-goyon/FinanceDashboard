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
- **`AreaSeries` has no `pointDelegate`** — stacked areas get transparent `LineSeries` "hover
  carriers" on each boundary; tooltip shows the layer's own (non-cumulative) value via
  `HoverPoint.overrideValues`.
- **Child `Component.onCompleted` runs before the parent's** — series filled by a page-level
  handler must be appended there, not in the child.
- **Sankey**: no Qt Graphs series type exists for it — `qml/components/MoneyFlow.qml` is a
  pure-QML Canvas port of QtGraphsExamples → flowchart.
- **Inline `component` declarations** must sit at the document root, not nested in a `Popup`.
- Donuts use `pieSize: 0.75` / `holeSize: 0.42` (`0.85` clipped the labels).
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
