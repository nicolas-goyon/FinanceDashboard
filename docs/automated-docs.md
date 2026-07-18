# Automated Documentation

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

Hand-maintaining a full file-by-file listing (like the old `ARCHITECTURE.MD` "Directory Map")
goes stale the moment a file is added, renamed, or removed and nobody remembers to update the
doc in the same pass. `tools/generate_docs.py` fixes the "list every file" part of that problem
by generating it from the source tree itself, on demand.

## The convention it reads

Most files in `src/` and `qml/` already open with a short banner comment, e.g.:

```cpp
// ── AccountController ─────────────────────────────────────────────────────────
//
// Session-wide source of truth for the accounts list — backs
// qml/pages/AccountsManagePage.qml ...
```

or the shorter one-line-banner form used by some `qml/components/*.qml` files:

```qml
// ── PillButton ───────────────────────────────────────────────────────────────
// The big colored action button used everywhere ...
```

This was already the de-facto style across the codebase before this doc pass — the script
formalizes it rather than inventing something new, so **no source file needs to change** for
its existing description to show up in the generated doc. A file with no banner is still
listed, just flagged as `(no banner comment yet — see docs/automated-docs.md)` so the tree stays
complete.

**When adding a new file**, give it a banner in this shape if you want it to show up with a
description:

```cpp
// ── ClassOrFileName ───────────────────────────────────────────────────────────
//
// One paragraph, plain prose, describing what this file is/does and what it's
// backed by or backs. Keep the first paragraph self-contained — the script only
// pulls the first paragraph after the banner line.
```

## Running it

```powershell
python tools/generate_docs.py          # regenerate docs/generated/source-tree.md
python tools/generate_docs.py --check  # exit 1 if the file is stale (no write) — for CI
```

Requires Python 3.9+, no third-party dependencies. `--check` is meant to be wired into a
pre-commit hook or a CI step later if this project ever gets one — today it's a manual "run
this before committing a structural change" step.

## What it generates today

`docs/generated/source-tree.md` — every `.h`/`.cpp`/`.qml` file under `src/` and `qml/`,
grouped by directory, each with its extracted description (or a note that none was found).
Regenerate it any time files move around; it's gitignored-free (checked in) so a diff shows
exactly what changed structurally.

## Known limitations (v1 of this script)

- Description extraction is banner-text extraction, not real parsing — a file with more than
  one `// ── Name ──` block (e.g. `models/Account.h`, which documents four structs in one file)
  will sometimes pick up a fragment from a later block instead of a clean first paragraph. Good
  enough for "what is this file, roughly," not a substitute for reading the file.
- Only scans `src/` and `qml/` — `tools/*.ps1`, `CMakeLists.txt`, and the top-level `.md` files
  aren't included (they're already indexed by `ARCHITECTURE.MD` itself).
- Doesn't distinguish MOCK vs. REAL data status, controller type (`QML_ELEMENT` vs.
  `QML_SINGLETON`), or DB schema — that level of detail still lives in the hand-written
  `docs/*.md` files (`docs/app-status.md`, `docs/account-management.md`, etc.) and is not a
  near-term goal for automation, since it requires judgment (design intent), not just extraction.

## Extending this later

If `docs/app-status.md`'s status table (MOCK vs. REAL, which controller backs which page) is
worth automating too, the natural next step is a second, stricter comment tag (e.g.
`// @status: MOCK` / `// @status: REAL`) that the script can key off directly, rather than
trying to infer it from prose. Not built yet — add it if keeping that table current by hand
becomes a recurring source of drift (see `TODO.md`'s "Documentation follow-ups").
