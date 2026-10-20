#!/usr/bin/env python3
"""release_flow.py — the git part of the release flow that CI runs.

Each command makes a local branch with one change, for a pull request into
develop. CI then pushes the branch, opens the pull request and starts CI on
it (see .github/workflows/ci.yml and CI-CD.md § The release flow).

    open-next X.Y.Z --develop REF
        After release/X.Y.Z is cut from develop: makes the branch
        chore/open-v<next> from REF and runs `version.py cut` on it, so that
        develop can take changes for the next version while the release
        branch is tested. Prints <next>. Prints nothing when develop already
        opened the next version. Run from the release branch: it first
        stops if develop changed vX.Y.Z after the cut (a pull request merged
        into develop with an entry under it).

    back-merge X.Y.Z --release SHA --develop REF
        After vX.Y.Z is released from SHA (on main): makes the branch
        chore/back-merge-vX.Y.Z from REF and merges SHA into it. The pull
        request then has no conflict. A conflict in version.md (a generated
        file) is solved: version.md is rendered again from the merged
        version.json. Any other conflict stops the command. If develop did
        not open the next version (its pull request was not merged before
        the release), `version.py bump patch` opens it. Prints the
        in-progress version of the result. Prints nothing when REF already
        contains SHA.

    channel --event EVENT --ref REF --run N [--pr N --labels JSON]
        The pre-release of a CI run (#129), for the package jobs and the
        pre-release job. Prints key=value lines for $GITHUB_OUTPUT: channel
        (experimental, preprod or none), publish (true or false), suffix,
        version (the version of version.json with the suffix) and tag.
        <run> is the run number of the workflow: the same in every job and
        every re-run of a run, and it grows with each run, so the versions
        of a channel sort in build order (SemVer: 0.2.2-exp.57 <
        0.2.2-exp.61.pr12 < 0.2.2-rc.70 < 0.2.2).
          - a push to develop: experimental, -exp.<run>, published;
          - a push to release/*: preprod, -rc.<run>, published;
          - a pull request: experimental, -exp.<run>.pr<pr>, published only
            with the label "experimental" (--labels, a JSON list of names);
          - anything else: none, no suffix, not published.

open-next and back-merge need a clean working tree. Stdlib only, like
version.py.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def run(cmd: list[str]) -> str:
    result = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8")
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        sys.exit(f"error: `{' '.join(cmd)}` failed (exit code {result.returncode})")
    return result.stdout


def git(*args: str) -> str:
    return run(["git", *args])


def version_py(*args: str) -> str:
    """Run tools/version.py of the working tree: after a switch, the version of that branch."""
    return run([sys.executable, str(REPO_ROOT / "tools" / "version.py"), *args])


def last_line(text: str) -> str:
    lines = text.strip().splitlines()
    return lines[-1].strip() if lines else ""


def commit_of(ref: str) -> str:
    return git("rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}").strip()


def current_at(commit: str) -> str:
    return json.loads(git("show", f"{commit}:version.json"))["current"]


def current_here() -> str:
    return json.loads((REPO_ROOT / "version.json").read_text(encoding="utf-8"))["current"]


def release_at(commit: str, version: str) -> dict | None:
    data = json.loads(git("show", f"{commit}:version.json"))
    return next((r for r in data["releases"] if r["version"] == version), None)


def require_clean() -> None:
    if git("status", "--porcelain", "--untracked-files=no").strip():
        sys.exit("error: the working tree has changes: commit them or start from a clean checkout")


def cmd_open_next(args: argparse.Namespace) -> int:
    version = args.version
    require_clean()
    here = current_here()
    if here != version:
        sys.exit(f"error: the release branch is for v{version}, but its version.json is "
                 f"v{here}. Name the release branch after the version that it releases "
                 f"(release/{here}).")
    develop = commit_of(args.develop)
    # After the cut, only the release branch changes vX.Y.Z. A pull request
    # whose CI ran before the cut can still put an entry under it on develop:
    # stop here, before the release, not after it.
    cut_point = git("merge-base", "HEAD", develop).strip()
    at_cut = release_at(cut_point, version)
    on_develop = release_at(develop, version)
    if at_cut is not None and (on_develop is None
                               or (not on_develop.get("date") and on_develop != at_cut)):
        sys.exit(f"error: develop changed v{version} after release/{version} was cut (at "
                 f"{cut_point[:12]}). Only the release branch changes it: on develop, move the "
                 f"new entries of v{version} to the next version, in a pull request.")
    develop_current = current_at(develop)
    if develop_current != version:
        print(f"develop is on v{develop_current}: the next version is open.", file=sys.stderr)
        return 0
    git("switch", "--quiet", "--detach", develop)
    next_version = last_line(version_py("cut"))
    git("switch", "--quiet", "-c", f"chore/open-v{next_version}")
    version_py("check")
    git("add", "version.json", "version.md")
    git("commit", "--quiet", "-m", f"chore(release): open v{next_version} on develop",
        "-m", f"release/{version} is cut. v{version} stays undated on develop until its "
              f"release: the back-merge from main gives it its date.")
    print(next_version)
    return 0


def cmd_back_merge(args: argparse.Namespace) -> int:
    version = args.version
    require_clean()
    release = commit_of(args.release)
    develop = commit_of(args.develop)
    if subprocess.run(["git", "merge-base", "--is-ancestor", release, develop],
                      cwd=REPO_ROOT).returncode == 0:
        print(f"develop already contains {release}: the back-merge is done.", file=sys.stderr)
        return 0
    released = current_at(release)
    if released != version:
        sys.exit(f"error: {release} releases v{released}, not v{version}")

    git("switch", "--quiet", "-c", f"chore/back-merge-v{version}", develop)
    merge = subprocess.run(["git", "merge", "--no-ff", "--no-edit", "-m",
                            f"chore(release): merge v{version} back into develop", release],
                           cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8")
    if merge.returncode != 0:
        conflicts = git("diff", "--name-only", "--diff-filter=U").split()
        if conflicts != ["version.md"]:
            subprocess.run(["git", "merge", "--abort"], cwd=REPO_ROOT, capture_output=True)
            sys.stderr.write(merge.stdout + merge.stderr)
            sys.exit(f"error: the merge of v{version} into develop conflicts in "
                     f"{', '.join(conflicts) or 'no file (see the git output above)'}. Merge it "
                     f"by hand: see CI-CD.md § The release flow.")
        version_py("render")
        git("add", "version.md")
        # --cleanup=strip: the "# Conflicts:" lines of the prepared message go.
        git("commit", "--quiet", "--no-edit", "--cleanup=strip")

    if current_here() == version:
        # develop did not open the next version (its pull request was not
        # merged before the release): open it here. The release job closes
        # that pull request.
        next_version = last_line(version_py("bump", "patch"))
        git("add", "version.json", "version.md")
        git("commit", "--quiet", "-m", f"chore(release): open v{next_version} after v{version}")
    # The released versions of main stay as main has them.
    version_py("check", "--base", release)
    print(current_here())
    return 0


# The label that makes a pull request publish its pre-release (#129).
EXPERIMENTAL_LABEL = "experimental"


def cmd_channel(args: argparse.Namespace) -> int:
    version = current_here()
    channel, suffix, publish = "none", "", False
    if args.event == "push" and args.ref == "refs/heads/develop":
        channel, suffix, publish = "experimental", f"-exp.{args.run}", True
    elif args.event == "push" and args.ref.startswith("refs/heads/release/"):
        channel, suffix, publish = "preprod", f"-rc.{args.run}", True
    elif args.event == "pull_request":
        if args.pr is None:
            sys.exit("error: a pull request needs --pr (its number)")
        labels = json.loads(args.labels) if args.labels else None
        channel, suffix = "experimental", f"-exp.{args.run}.pr{args.pr}"
        publish = EXPERIMENTAL_LABEL in (labels or [])
    full = version + suffix
    print(f"channel={channel}")
    print(f"publish={'true' if publish else 'false'}")
    print(f"suffix={suffix}")
    print(f"version={full}")
    print(f"tag=v{full}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="release_flow.py",
        description="Make the branches of the release flow that CI pushes (see CI-CD.md).",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    open_next = sub.add_parser("open-next", help="after release/X.Y.Z is cut: open the next "
                                                 "version on a branch from develop")
    open_next.add_argument("version", help="X.Y.Z, the version of the release branch")
    open_next.add_argument("--develop", required=True, help="the develop commit (a ref or a SHA)")
    open_next.set_defaults(func=cmd_open_next)

    back_merge = sub.add_parser("back-merge", help="after vX.Y.Z is released: merge it into a "
                                                   "branch from develop")
    back_merge.add_argument("version", help="X.Y.Z, the released version")
    back_merge.add_argument("--release", required=True, help="the released commit on main")
    back_merge.add_argument("--develop", required=True, help="the develop commit (a ref or a SHA)")
    back_merge.set_defaults(func=cmd_back_merge)

    channel = sub.add_parser("channel", help="the pre-release of a CI run: channel, suffix, "
                                             "version and tag (#129)")
    channel.add_argument("--event", required=True, help="github.event_name")
    channel.add_argument("--ref", required=True, help="github.ref")
    channel.add_argument("--run", required=True, type=int, help="github.run_number")
    channel.add_argument("--pr", type=int, help="the number of the pull request")
    channel.add_argument("--labels", help="the labels of the pull request, a JSON list of names")
    channel.set_defaults(func=cmd_channel)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
