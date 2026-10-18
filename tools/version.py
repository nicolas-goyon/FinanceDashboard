#!/usr/bin/env python3
"""version.py — single source of truth for the app version and the changelog.

`version.json` at the repo root holds both:

  * `current` — the version currently *in progress* (not yet released). This is
    the number `CMakeLists.txt` feeds to `project(FinanceDashboard VERSION ...)`,
    so the build, the installers and the release tag can never drift apart.
  * `releases` — the changelog, one entry per version, newest first, in the same
    Added / Changed / Fixed / Docs shape `version.md` always used.

`version.md` is *generated* from this file (`version.py render`) — edit
`version.json` (or use `version.py add`), never `version.md` directly. CI runs
`version.py check` and fails if the two disagree.

Release flow (gitflow, see CI-CD.md):

    on release/0.2.1:  version.py release   -> stamps today's date on 0.2.1
    merge into main:   CI publishes v0.2.1 (tag + GitHub release), using
                       version.py current / notes
    CI, after that:    version.py bump patch on a branch from main
                       -> opens 0.2.2; CI opens a PR from it into develop

Day-to-day, log a change the moment you make it — same maintenance rule as
before, just a different command:

    python tools/version.py add Added "`Foo.qml` — the new thing it does."
    python tools/version.py add Fixed "Bar no longer does the wrong thing."
    python tools/version.py add Added --group "Navigation & shell" "..."
    python tools/version.py summary "One line describing this version."
    python tools/version.py render        # refresh version.md

No third-party dependencies — stdlib only, same as tools/generate_docs.py.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import re
import subprocess
import sys
import textwrap
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
VERSION_JSON = REPO_ROOT / "version.json"
VERSION_MD = REPO_ROOT / "version.md"

WRAP_WIDTH = 95

SEMVER_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")

# Canonical section order. Sections not listed here are kept, in the order they
# first appear, after the known ones.
SECTION_ORDER = ["Added", "Changed", "Fixed", "Removed", "Docs"]

MD_PREAMBLE = """# Version History — Finance Dashboard

<!-- GENERATED FILE — do not edit by hand.
     Source of truth: version.json. Regenerate with `python tools/version.py render`.
     Add an entry with `python tools/version.py add <Section> "<text>"`.
     CI (`version.py check`) fails if this file is out of sync with version.json. -->

This file is the single top-level changelog for the app. It tracks the current version and
every previous one, in the same style as a standard `CHANGELOG.md` (newest first). It is meant
to answer "what changed and when," at a glance — for *why* something is built the way it is,
see `ARCHITECTURE.MD`; for how it is built, tested and released, see `CI-CD.md`.

**Format:** each version gets a heading, a one-line summary, and `Added` / `Changed` / `Fixed`
/ `Docs` subsections (omit any that don't apply). The version number lives in `version.json`
and is read from there by `CMakeLists.txt` — it is never typed twice.

**Maintenance rule** (see the project instructions): every reasonable change gets logged here
under the current in-progress version, at the time it's made — not reconstructed after the
fact. Small fixes to a feature that's still unreleased/in-progress in the *same* version don't
need their own extra line; fold them into the feature's own entry instead of listing "fixed a
bug in the thing I just added two lines above."

**Releases** follow gitflow. A `release/<x.y.z>` branch stamps the date here
(`version.py release`). Its merge into `main` publishes the GitHub release (setup, portable
builds, source archives). Then CI opens a pull request into `develop` that opens the next patch
version as the new in-progress one. See `CI-CD.md`.
"""


# ─────────────────────────────────────────────────────────────────────────────
# Model
# ─────────────────────────────────────────────────────────────────────────────


def load() -> dict[str, Any]:
    if not VERSION_JSON.exists():
        sys.exit(f"error: {VERSION_JSON} not found — run `version.py import-md` once to create it.")
    with VERSION_JSON.open(encoding="utf-8") as handle:
        return json.load(handle)


def save(data: dict[str, Any]) -> None:
    with VERSION_JSON.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(data, handle, indent=2, ensure_ascii=False)
        handle.write("\n")


def find_release(data: dict[str, Any], version: str) -> dict[str, Any]:
    for release in data["releases"]:
        if release["version"] == version:
            return release
    sys.exit(f"error: version {version} not found in version.json")


def current_release(data: dict[str, Any]) -> dict[str, Any]:
    return find_release(data, data["current"])


def get_section(release: dict[str, Any], name: str, create: bool = False) -> dict[str, Any] | None:
    for section in release["sections"]:
        if section["name"].lower() == name.lower():
            return section
    if not create:
        return None
    section = {"name": name, "groups": [{"title": None, "entries": []}]}
    release["sections"].append(section)
    # Keep the canonical order (Added / Changed / Fixed / Removed / Docs, then the rest).
    release["sections"].sort(key=lambda s: (
        SECTION_ORDER.index(s["name"]) if s["name"] in SECTION_ORDER else len(SECTION_ORDER)
    ))
    return section


# ─────────────────────────────────────────────────────────────────────────────
# Rendering — version.json -> markdown
# ─────────────────────────────────────────────────────────────────────────────


def wrap_entry(text: str, bullet: str = "- ") -> str:
    """Wrap one changelog bullet to WRAP_WIDTH, indenting continuation lines."""
    return textwrap.fill(
        text,
        width=WRAP_WIDTH,
        initial_indent=bullet,
        subsequent_indent="  ",
        break_long_words=False,
        break_on_hyphens=False,
    )


def render_release(release: dict[str, Any], *, heading_level: int = 2, with_heading: bool = True,
                   current: str | None = None) -> str:
    out: list[str] = []
    if with_heading:
        heading = "#" * heading_level
        version = release["version"]
        date = release.get("date")
        title = release.get("title")
        if title:
            suffix = f" — {title}"
        elif date:
            suffix = ""
        else:
            # Only the in-progress version is "current". `check` refuses any
            # other undated release, so the second label is a safety net only.
            suffix = " (current)" if version == current else " (unreleased)"
        stamp = f" — {date}" if date else ""
        out.append(f"{heading} v{version}{stamp}{suffix}")
        out.append("")

    summary = (release.get("summary") or "").strip()
    if summary:
        out.append(textwrap.fill(summary, width=WRAP_WIDTH, break_long_words=False,
                                 break_on_hyphens=False))
        out.append("")

    for section in release["sections"]:
        entries_total = sum(len(g["entries"]) for g in section["groups"])
        if entries_total == 0:
            continue
        out.append(f"{'#' * (heading_level + 1)} {section['name']}")
        for group in section["groups"]:
            if not group["entries"]:
                continue
            if group.get("title"):
                out.append(f"**{group['title']}**")
            for entry in group["entries"]:
                out.append(wrap_entry(entry))
            out.append("")
        if out and out[-1] == "":
            out.pop()
        out.append("")

    while out and out[-1] == "":
        out.pop()
    if len(out) <= (1 if with_heading else 0):
        if with_heading:
            out.append("")
        out.append("_No changes logged yet — add one with "
                   "`python tools/version.py add <Section> \"<text>\"`._")
    return "\n".join(out)


def render_markdown(data: dict[str, Any]) -> str:
    parts = [MD_PREAMBLE.rstrip(), "", "---", ""]
    for release in data["releases"]:
        parts.append(render_release(release, current=data["current"]))
        parts.append("")
        parts.append("---")
        parts.append("")
    # Drop the trailing separator.
    while parts and parts[-1] in ("", "---"):
        parts.pop()
    return "\n".join(parts).rstrip() + "\n"


# ─────────────────────────────────────────────────────────────────────────────
# Parsing — markdown -> version.json (one-shot migration, kept for reference)
# ─────────────────────────────────────────────────────────────────────────────


def parse_markdown(text: str) -> dict[str, Any]:
    """Parse a version.md in this project's format into the version.json model."""
    releases: list[dict[str, Any]] = []
    release: dict[str, Any] | None = None
    section: dict[str, Any] | None = None
    group: dict[str, Any] | None = None
    pending: list[str] | None = None       # lines of the bullet being accumulated
    summary_lines: list[str] = []

    def flush_entry() -> None:
        nonlocal pending
        if pending is not None and group is not None:
            group["entries"].append(" ".join(part.strip() for part in pending).strip())
        pending = None

    def flush_summary() -> None:
        if release is not None and summary_lines:
            release["summary"] = " ".join(line.strip() for line in summary_lines).strip()
        summary_lines.clear()

    for raw in text.splitlines():
        line = raw.rstrip()

        version_match = re.match(r"^##\s+v(\d+\.\d+\.\d+)\s*(.*)$", line)
        if version_match:
            flush_entry()
            flush_summary()
            version = version_match.group(1)
            title = version_match.group(2).strip().lstrip("—-").strip()
            date = None
            if title:
                date_match = re.match(r"^(\d{4}-\d{2}-\d{2})\s*(?:—\s*(.*))?$", title)
                if date_match:
                    date, title = date_match.group(1), (date_match.group(2) or "").strip()
            if title.lower() in ("(current)", "current"):
                title = ""
            release = {"version": version, "date": date, "title": title or None,
                       "summary": "", "sections": []}
            releases.append(release)
            section = group = None
            continue

        if release is None:
            continue  # still in the file preamble

        section_match = re.match(r"^###\s+(.+?)\s*$", line)
        if section_match:
            flush_entry()
            flush_summary()
            section = {"name": section_match.group(1).strip(),
                       "groups": [{"title": None, "entries": []}]}
            release["sections"].append(section)
            group = section["groups"][0]
            continue

        if section is not None:
            group_match = re.match(r"^\*\*(.+?)\*\*\s*$", line)
            if group_match:
                flush_entry()
                group = {"title": group_match.group(1).strip(), "entries": []}
                section["groups"].append(group)
                continue

            bullet_match = re.match(r"^[-*]\s+(.*)$", line)
            if bullet_match:
                flush_entry()
                pending = [bullet_match.group(1)]
                continue

            if line.strip() == "" or line.strip() == "---":
                flush_entry()
                continue

            if pending is not None and line.startswith(("  ", "\t")):
                pending.append(line)
                continue

            flush_entry()
            continue

        # Between the version heading and the first "###" — that's the summary.
        if line.strip() in ("", "---"):
            continue
        summary_lines.append(line)

    flush_entry()
    flush_summary()

    for rel in releases:
        rel["sections"] = [s for s in rel["sections"]
                           if any(g["entries"] for g in s["groups"])]
        for sec in rel["sections"]:
            sec["groups"] = [g for g in sec["groups"] if g["entries"]]

    current = next((r["version"] for r in releases if not r["date"]),
                   releases[0]["version"] if releases else "0.0.0")
    return {"current": current, "releases": releases}


# ─────────────────────────────────────────────────────────────────────────────
# Commands
# ─────────────────────────────────────────────────────────────────────────────


def cmd_current(_args: argparse.Namespace) -> int:
    print(load()["current"])
    return 0


def cmd_notes(args: argparse.Namespace) -> int:
    data = load()
    version = args.version or data["current"]
    release = find_release(data, version)
    print(render_release(release, heading_level=2, with_heading=False))
    return 0


def require_in_progress(release: dict[str, Any]) -> None:
    """Refuse to change a version that is already released (dated)."""
    if release.get("date"):
        sys.exit(f"error: v{release['version']} is released ({release['date']}). Open the next "
                 f"version first: `python tools/version.py bump`.")


def cmd_add(args: argparse.Namespace) -> int:
    data = load()
    release = current_release(data)
    require_in_progress(release)
    section = get_section(release, args.section, create=True)
    assert section is not None
    title = args.group
    target = next((g for g in section["groups"] if (g.get("title") or None) == (title or None)), None)
    if target is None:
        target = {"title": title, "entries": []}
        section["groups"].append(target)
    target["entries"].append(args.text.strip())
    save(data)
    VERSION_MD.write_text(render_markdown(data), encoding="utf-8", newline="\n")
    print(f"added to v{release['version']} § {section['name']}: {args.text.strip()[:70]}")
    return 0


def cmd_summary(args: argparse.Namespace) -> int:
    data = load()
    release = current_release(data)
    require_in_progress(release)
    release["summary"] = args.text.strip()
    save(data)
    VERSION_MD.write_text(render_markdown(data), encoding="utf-8", newline="\n")
    print(f"summary set for v{release['version']}")
    return 0


def cmd_release(args: argparse.Namespace) -> int:
    data = load()
    release = current_release(data)
    if release.get("date") and not args.force:
        sys.exit(f"error: v{release['version']} is already dated {release['date']} "
                 f"(use --force to overwrite)")
    release["date"] = args.date or _dt.date.today().isoformat()
    save(data)
    VERSION_MD.write_text(render_markdown(data), encoding="utf-8", newline="\n")
    print(release["date"])
    return 0


def latest_released(data: dict[str, Any]) -> str | None:
    """The highest version that has a date, or None."""
    dated = [r["version"] for r in data["releases"]
             if r.get("date") and SEMVER_RE.match(r["version"])]
    if not dated:
        return None
    return max(dated, key=lambda v: tuple(int(p) for p in SEMVER_RE.match(v).groups()))


def cmd_bump(args: argparse.Namespace) -> int:
    """Open the next version.

    If the current version is released (dated), insert a new undated version
    above it. If the current version is still in progress (undated), rename it
    instead, so that its entries stay together and there is never more than one
    in-progress version: a `bump minor` on an in-progress 0.2.2 makes it 0.3.0.
    The next number is counted from the latest released version, so a rename
    from 0.2.2 to 0.3.0 and back to a patch gives 0.2.2 again, not 0.3.1.
    """
    data = load()
    release = current_release(data)
    in_progress = not release.get("date")
    base = latest_released(data) if in_progress else data["current"]
    if base is None:
        base = data["current"]
        if in_progress:
            sys.exit(f"error: v{base} is the only version and is not released; "
                     f"there is nothing to count the next version from")
    match = SEMVER_RE.match(base)
    if not match:
        sys.exit(f"error: version {base!r} is not X.Y.Z")
    major, minor, patch = (int(part) for part in match.groups())
    if args.part == "major":
        major, minor, patch = major + 1, 0, 0
    elif args.part == "minor":
        minor, patch = minor + 1, 0
    else:
        patch += 1
    new_version = f"{major}.{minor}.{patch}"
    if in_progress and new_version == release["version"]:
        print(new_version)
        return 0
    if any(r["version"] == new_version for r in data["releases"]):
        sys.exit(f"error: v{new_version} already exists in version.json")
    if in_progress:
        release["version"] = new_version
    else:
        data["releases"].insert(0, {
            "version": new_version,
            "date": None,
            "title": None,
            "summary": "",
            "sections": [],
        })
    data["current"] = new_version
    save(data)
    VERSION_MD.write_text(render_markdown(data), encoding="utf-8", newline="\n")
    print(new_version)
    return 0


def cmd_render(_args: argparse.Namespace) -> int:
    data = load()
    VERSION_MD.write_text(render_markdown(data), encoding="utf-8", newline="\n")
    print(f"wrote {VERSION_MD.relative_to(REPO_ROOT)}")
    return 0


def load_at(ref: str) -> dict[str, Any]:
    """version.json as it is in a git ref (for example origin/main)."""
    try:
        text = subprocess.run(["git", "show", f"{ref}:version.json"], cwd=REPO_ROOT,
                              check=True, capture_output=True, text=True, encoding="utf-8").stdout
    except (OSError, subprocess.CalledProcessError) as exc:
        sys.exit(f"error: cannot read version.json at {ref!r}: {exc}")
    return json.loads(text)


def cmd_check(args: argparse.Namespace) -> int:
    data = load()
    expected = render_markdown(data)
    actual = VERSION_MD.read_text(encoding="utf-8") if VERSION_MD.exists() else ""
    if expected != actual:
        print("version.md is out of sync with version.json — run "
              "`python tools/version.py render` and commit the result.", file=sys.stderr)
        return 1
    seen: set[str] = set()
    for rel in data["releases"]:
        if not SEMVER_RE.match(rel["version"]):
            print(f"invalid version string: {rel['version']!r}", file=sys.stderr)
            return 1
        if rel["version"] in seen:
            print(f"duplicate version: {rel['version']}", file=sys.stderr)
            return 1
        seen.add(rel["version"])
    if data["current"] not in seen:
        print(f"current version v{data['current']} is not in the releases list", file=sys.stderr)
        return 1
    # At most one version is in progress, and it is the current one. On develop
    # the current version is in progress; on main (after a release) it is dated.
    undated = [r["version"] for r in data["releases"] if not r.get("date")]
    if len(undated) > 1:
        print(f"more than one version has no date: {', '.join('v' + v for v in undated)}. "
              f"Only the current version can be in progress.", file=sys.stderr)
        return 1
    if undated and undated[0] != data["current"]:
        print(f"v{undated[0]} has no date but is not the current version "
              f"(v{data['current']}).", file=sys.stderr)
        return 1
    # A version that is released on the base branch cannot change any more.
    if args.base:
        base = {r["version"]: r for r in load_at(args.base)["releases"]}
        here = {r["version"]: r for r in data["releases"]}
        for version, rel in base.items():
            if not rel.get("date"):
                continue
            if version not in here:
                print(f"v{version} is released on {args.base} but missing here.", file=sys.stderr)
                return 1
            if not here[version].get("date"):
                # develop before the back-merge PR: not a change, only behind.
                print(f"note: v{version} is released on {args.base}; merge the back-merge "
                      f"pull request into this branch.")
                continue
            if here[version] != rel:
                print(f"v{version} is released on {args.base}, and this branch changes it. "
                      f"Log the change under the current version (v{data['current']}).",
                      file=sys.stderr)
                return 1
    print(f"ok — version.json and version.md agree (current: v{data['current']})")
    return 0


def cmd_import_md(args: argparse.Namespace) -> int:
    if VERSION_JSON.exists() and not args.force:
        sys.exit(f"error: {VERSION_JSON.name} already exists (use --force to overwrite)")
    data = parse_markdown(VERSION_MD.read_text(encoding="utf-8"))
    save(data)
    print(f"wrote {VERSION_JSON.name}: {len(data['releases'])} release(s), "
          f"current v{data['current']}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="version.py",
        description="Read and update version.json — the app's version and changelog.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("current", help="print the in-progress version (X.Y.Z)").set_defaults(func=cmd_current)

    notes = sub.add_parser("notes", help="print release-notes markdown for a version")
    notes.add_argument("--version", help="defaults to the current in-progress version")
    notes.set_defaults(func=cmd_notes)

    add = sub.add_parser("add", help="append a changelog entry to the current version")
    add.add_argument("section", help="Added / Changed / Fixed / Removed / Docs")
    add.add_argument("text", help="the entry, markdown allowed")
    add.add_argument("--group", help="optional bold sub-heading to file it under")
    add.set_defaults(func=cmd_add)

    summary = sub.add_parser("summary", help="set the one-line summary of the current version")
    summary.add_argument("text")
    summary.set_defaults(func=cmd_summary)

    release = sub.add_parser("release", help="stamp today's date on the current version")
    release.add_argument("--date", help="YYYY-MM-DD (defaults to today)")
    release.add_argument("--force", action="store_true")
    release.set_defaults(func=cmd_release)

    bump = sub.add_parser("bump", help="open the next version as in-progress")
    bump.add_argument("part", nargs="?", default="patch", choices=["major", "minor", "patch"])
    bump.set_defaults(func=cmd_bump)

    sub.add_parser("render", help="regenerate version.md from version.json").set_defaults(func=cmd_render)
    check = sub.add_parser("check", help="fail if version.md is stale or the model is invalid")
    check.add_argument("--base", help="git ref (for example origin/main): fail if this branch "
                                      "changes a version that is released there")
    check.set_defaults(func=cmd_check)

    import_md = sub.add_parser("import-md", help="one-shot: build version.json from version.md")
    import_md.add_argument("--force", action="store_true")
    import_md.set_defaults(func=cmd_import_md)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
