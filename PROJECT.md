# Product Notes — Finance Dashboard

> **Audience:** product/design reference. What the app should do, independent of the current code.
> For code structure and build info, see `ARCHITECTURE.MD` / `README.md`.

## Context

Personal finance analytics app for:

- 2 main bank (checking) accounts
- Savings accounts (Livret A, LDDS, assurance vie, …)
- 1 trading account

**No auto-fetching from banks.** Data comes from manual input and monthly reports. The natural data model is therefore a **monthly snapshot per account**, plus optional categorized transactions. All charts are month-bucketed (no daily granularity), typically 12–24 month windows with a range selector.

---

## Pages

### 1. Overview (Net Worth)

Landing page — "how am I doing overall?"

- KPI cards: total net worth, month-over-month change (€ and %), savings rate this month, months of emergency fund
- Stacked area chart: net worth over time, split by account type (checking / savings / trading)
- Donut: current allocation across account types
- Bar chart: monthly net change over last 12 months (green/red bars)

### 2. Cash Flow (checking accounts)

"Where does my money go?"

- Grouped bars: income vs expenses per month
- Line: rolling 3-month average spending
- Donut or horizontal bars: spending by category for a selected month
- Stacked bars: category spending across months (spot category drift)
- Table: biggest expenses of the month

### 3. Savings (livrets, assurance vie)

"Is my safety net growing?"

- Line per account: balance over time
- Progress bars toward livret ceilings (Livret A 22 950 €, LDDS 12 000 €) and personal goals
- Bar: interest/gains earned per year per account
- KPI: blended yield across all savings

### 4. Investments (trading account)

"Am I actually making money, or just depositing it?"

- Line chart, two series: **portfolio value vs cumulative deposits** — the gap is real P&L (the single most useful investing chart)
- Donut: allocation by asset / asset class
- Bar: monthly/yearly performance %
- KPIs: total invested, unrealized P&L, simple return %

### 5. Projects (mid/long-term goals)

"Can I afford a house / car / X, and do I need a loan?"

Each project has: name, target amount, target date, savings already earmarked, monthly contribution.

Per project:

- KPI cards: amount still needed, projected completion date at current contribution rate, funding gap at target date
- Line chart: projected savings vs target over time (projection from current contribution rate)
- **Loan simulator** for the funding gap: user enters candidate rate(s) and duration → monthly payment, total interest cost, total cost of credit
  - Comparison table/bars: 2–3 loan scenarios side by side (e.g. 10y vs 15y vs 20y, or rate A vs rate B)
  - "Save longer vs borrow now" comparison: cost of waiting N more months vs interest paid by borrowing today
- Rates are **entered manually** (no market data fetching)

### 6. Data Entry / Reports

- Form: monthly snapshot per account
- Optional transaction entry with category
- History table of past reports — **editable**, including **re-categorizing past transactions** (see Category System)
- Annual summary view (year vs year comparison)

---

## Category System

### Editing

Categories of previously entered transactions/inputs can be edited at any time from the history table. Edits recompute all affected aggregates (cash-flow charts, monthly summaries).

### Prediction

When a new transaction is entered, the app suggests a category automatically:

1. **Exact/label match first:** if a transaction with the same (normalized) label was already categorized, reuse that category.
2. **Fallback — simple classifier:** keyword/token matching or Naive Bayes over the user's own historical labeled transactions (tokens from label + amount range as features). No external ML dependency — plain C++.
3. The suggestion is **always editable** before saving; user corrections feed back into the training data, so accuracy improves with use.
4. Re-categorizing past entries (see above) also updates the classifier's data.

---

## Design Notes / Open Decisions

- Cash-flow tracking depth: **category-level** (chosen — enables Cash Flow page + prediction) vs totals-only.
- Monthly granularity everywhere; avoid daily-looking charts.
- Emergency-fund KPI needs a user-defined "monthly essential expenses" setting.
- Loan math: standard annuity formula (fixed-rate amortization). Insurance (assurance emprunteur) as optional flat % input for realism on house projects.
