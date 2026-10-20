#!/usr/bin/env python3
"""Tests of version.py and release_flow.py (stdlib unittest).

    python -m unittest discover -s tools -p "test_*.py"

The first class tests the version model on temporary files. The second one
runs a whole release cycle in a temporary git repository, with the same
commands as CI: cut, fixes on the release branch, the merge into main and
the back-merge. It needs git.
"""

from __future__ import annotations

import contextlib
import copy
import io
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import version as vp  # noqa: E402  (after the sys.path change)


def release(version: str, date: str | None = None, entries: list[str] | None = None,
            section: str = "Added") -> dict:
    sections = [{"name": section, "groups": [{"title": None, "entries": list(entries)}]}] \
        if entries else []
    return {"version": version, "date": date, "title": None, "summary": "", "sections": sections}


def develop_data() -> dict:
    return {"current": "0.2.2", "releases": [
        release("0.2.2", entries=["A new thing."]),
        release("0.2.1", date="2026-10-19", entries=["An old thing."]),
    ]}


class VersionModelTest(unittest.TestCase):
    """version.py on a version.json in a temporary folder."""

    def setUp(self) -> None:
        self.folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.folder)
        self.saved = (vp.VERSION_JSON, vp.VERSION_MD, vp.load_at)
        vp.VERSION_JSON = self.folder / "version.json"
        vp.VERSION_MD = self.folder / "version.md"
        self.bases: dict[str, dict] = {}
        vp.load_at = lambda ref: json.loads(json.dumps(self.bases[ref]))
        self.addCleanup(self.restore)

    def restore(self) -> None:
        vp.VERSION_JSON, vp.VERSION_MD, vp.load_at = self.saved

    def write(self, data: dict) -> None:
        vp.save(data)
        vp.VERSION_MD.write_text(vp.render_markdown(data), encoding="utf-8", newline="\n")

    def run_main(self, *argv: str) -> tuple[int, str, str]:
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = vp.main(list(argv))
        return code, out.getvalue(), err.getvalue()

    def versions(self) -> list[str]:
        return [r["version"] for r in vp.load()["releases"]]

    def test_cut_opens_the_next_version_and_keeps_the_release(self) -> None:
        self.write(develop_data())
        code, out, _ = self.run_main("cut")
        self.assertEqual((code, out.strip()), (0, "0.2.3"))
        data = vp.load()
        self.assertEqual(data["current"], "0.2.3")
        self.assertEqual(self.versions(), ["0.2.3", "0.2.2", "0.2.1"])
        self.assertEqual(vp.find_release(data, "0.2.2"), develop_data()["releases"][0])
        markdown = vp.VERSION_MD.read_text(encoding="utf-8")
        self.assertIn("## v0.2.3 (current)", markdown)
        self.assertIn("## v0.2.2 (release in progress)", markdown)
        self.assertEqual(self.run_main("check")[0], 0)

    def test_cut_minor(self) -> None:
        self.write(develop_data())
        self.assertEqual(self.run_main("cut", "minor")[1].strip(), "0.3.0")

    def test_one_release_branch_at_a_time(self) -> None:
        self.write(develop_data())
        self.run_main("cut")
        with self.assertRaises(SystemExit):
            self.run_main("cut")

    def test_cut_needs_an_in_progress_version(self) -> None:
        data = develop_data()
        data["releases"][0]["date"] = "2026-10-30"
        self.write(data)
        with self.assertRaises(SystemExit):
            self.run_main("cut")

    def test_after_the_cut_add_logs_under_the_next_version(self) -> None:
        self.write(develop_data())
        self.run_main("cut")
        self.run_main("add", "Fixed", "Something for the next version.")
        data = vp.load()
        self.assertEqual(vp.find_release(data, "0.2.3")["sections"][0]["groups"][0]["entries"],
                         ["Something for the next version."])
        self.assertEqual(vp.find_release(data, "0.2.2"), develop_data()["releases"][0])

    def test_bump_counts_from_the_version_in_the_release_branch(self) -> None:
        self.write(develop_data())
        self.run_main("cut")
        self.assertEqual(self.run_main("bump", "patch")[1].strip(), "0.2.3")
        self.assertEqual(self.run_main("bump", "minor")[1].strip(), "0.3.0")
        self.assertEqual(self.versions(), ["0.3.0", "0.2.2", "0.2.1"])
        self.assertEqual(self.run_main("check")[0], 0)

    def test_check_refuses_a_second_undated_version(self) -> None:
        self.write({"current": "0.2.3", "releases": [
            release("0.2.3"), release("0.2.2"), release("0.2.1")]})
        self.assertEqual(self.run_main("check")[0], 1)

    def test_check_refuses_an_undated_version_lower_down(self) -> None:
        self.write({"current": "0.2.3", "releases": [
            release("0.2.3"), release("0.2.2", date="2026-10-30"), release("0.2.1")]})
        self.assertEqual(self.run_main("check")[0], 1)

    def test_check_refuses_an_undated_version_below_a_released_one(self) -> None:
        self.write({"current": "0.2.2", "releases": [
            release("0.2.2", date="2026-10-30"), release("0.2.1")]})
        self.assertEqual(self.run_main("check")[0], 1)

    def test_check_refuses_an_undated_version_above_the_current_one(self) -> None:
        self.write({"current": "0.2.2", "releases": [
            release("0.2.2"), release("0.2.3"), release("0.2.1", date="2026-10-19")]})
        self.assertEqual(self.run_main("check")[0], 1)

    def test_check_dated(self) -> None:
        self.write(develop_data())
        self.assertEqual(self.run_main("check", "--dated")[0], 1)
        data = develop_data()
        data["releases"][0]["date"] = "2026-10-30"
        self.write(data)
        self.assertEqual(self.run_main("check", "--dated")[0], 0)

    def test_check_base_keeps_the_version_in_a_release_branch(self) -> None:
        self.write(develop_data())
        self.run_main("cut")
        self.bases["develop"] = vp.load()

        self.run_main("add", "Added", "For the next version.")
        self.assertEqual(self.run_main("check", "--base", "develop")[0], 0)

        data = vp.load()
        vp.find_release(data, "0.2.2")["sections"][0]["groups"][0]["entries"].append("Late.")
        self.write(data)
        code, _, err = self.run_main("check", "--base", "develop")
        self.assertEqual(code, 1)
        self.assertIn("is in a release branch on develop", err)

        # After the back-merge, the version has its date from main: accepted.
        data = copy.deepcopy(self.bases["develop"])
        vp.find_release(data, "0.2.2")["date"] = "2026-10-30"
        vp.find_release(data, "0.2.2")["sections"][0]["groups"][0]["entries"].append("A fix.")
        self.write(data)
        self.assertEqual(self.run_main("check", "--base", "develop")[0], 0)

    def test_uncut_puts_the_release_branch_version_back_in_progress(self) -> None:
        self.write(develop_data())
        self.run_main("cut")
        self.run_main("add", "Added", "For the next version.")
        self.run_main("add", "Fixed", "A fix for the next version.")
        code, out, _ = self.run_main("uncut")
        self.assertEqual((code, out.strip()), (0, "0.2.2"))
        data = vp.load()
        self.assertEqual(data["current"], "0.2.2")
        self.assertEqual(self.versions(), ["0.2.2", "0.2.1"])
        sections = {s["name"]: s["groups"][0]["entries"]
                    for s in vp.find_release(data, "0.2.2")["sections"]}
        self.assertEqual(sections, {"Added": ["A new thing.", "For the next version."],
                                    "Fixed": ["A fix for the next version."]})
        self.assertEqual(self.run_main("check")[0], 0)
        with self.assertRaises(SystemExit):
            self.run_main("uncut")

    def test_check_base_keeps_released_versions(self) -> None:
        self.bases["main"] = {"current": "0.2.1",
                              "releases": [release("0.2.1", date="2026-10-19", entries=["x"])]}
        data = develop_data()
        self.write(data)
        self.assertEqual(self.run_main("check", "--base", "main")[0], 1)
        data["releases"][1]["sections"][0]["groups"][0]["entries"] = ["x"]
        self.write(data)
        self.assertEqual(self.run_main("check", "--base", "main", "--base", "main")[0], 0)


class ReleaseFlowTest(unittest.TestCase):
    """A whole release cycle in a temporary git repository."""

    def setUp(self) -> None:
        if shutil.which("git") is None:
            self.skipTest("git is not installed")
        self.repo = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.repo, ignore_errors=True)
        (self.repo / "tools").mkdir()
        for name in ("version.py", "release_flow.py"):
            shutil.copy(TOOLS / name, self.repo / "tools" / name)
        self.git("init", "--quiet", "--initial-branch=main")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        (self.repo / "version.json").write_text(json.dumps({"current": "0.2.1", "releases": [
            release("0.2.1", date="2026-10-19", entries=["An old thing."])]}, indent=2) + "\n",
            encoding="utf-8")
        self.version("render")
        self.commit("v0.2.1")
        self.git("switch", "--quiet", "-c", "develop")
        self.version("bump", "patch")
        self.version("add", "Added", "A thing for 0.2.2.")
        self.commit("open v0.2.2")

    def git(self, *args: str) -> str:
        return subprocess.run(["git", *args], cwd=self.repo, check=True, capture_output=True,
                              text=True, encoding="utf-8").stdout

    def tool(self, script: str, *args: str, check: bool = True) -> subprocess.CompletedProcess:
        result = subprocess.run([sys.executable, f"tools/{script}", *args], cwd=self.repo,
                                capture_output=True, text=True, encoding="utf-8")
        if check and result.returncode != 0:
            self.fail(f"{script} {' '.join(args)} failed:\n{result.stdout}{result.stderr}")
        return result

    def version(self, *args: str) -> str:
        return self.tool("version.py", *args).stdout.strip()

    def commit(self, message: str) -> None:
        self.git("add", "-A")
        self.git("commit", "--quiet", "-m", message)

    def data(self, ref: str | None = None) -> dict:
        if ref is None:
            return json.loads((self.repo / "version.json").read_text(encoding="utf-8"))
        return json.loads(self.git("show", f"{ref}:version.json"))

    def merge(self, branch: str, into: str) -> None:
        """A merge commit, as GitHub makes it."""
        self.git("switch", "--quiet", into)
        self.git("merge", "--quiet", "--no-ff", "--no-edit", branch)

    def release_branch_with_a_fix(self) -> None:
        self.git("switch", "--quiet", "release/0.2.2")
        self.version("add", "Fixed", "A fix found during the tests of the release.")
        self.version("summary", "The release of the tests.")
        self.version("release", "--date", "2026-10-30")
        self.commit("chore(release): v0.2.2")
        self.merge("release/0.2.2", "main")

    def back_merge(self) -> str:
        released = self.git("rev-parse", "main").strip()
        self.git("switch", "--quiet", "--detach", released)   # CI checks out main
        out = self.tool("release_flow.py", "back-merge", "0.2.2",
                        "--release", released, "--develop", "develop").stdout.strip()
        self.assertEqual(self.git("branch", "--show-current").strip(), "chore/back-merge-v0.2.2")
        return out

    def test_release_cycle_with_a_cut(self) -> None:
        self.git("switch", "--quiet", "-c", "release/0.2.2")
        self.assertEqual(self.tool("release_flow.py", "open-next", "0.2.2",
                                   "--develop", "develop").stdout.strip(), "0.2.3")
        self.assertEqual(self.git("branch", "--show-current").strip(), "chore/open-v0.2.3")
        self.merge("chore/open-v0.2.3", "develop")

        # develop takes changes for the next version at once.
        self.version("add", "Added", "A thing for 0.2.3.")
        self.commit("feat: a thing for 0.2.3")
        self.assertEqual(self.tool("version.py", "check", "--base", "main").returncode, 0)

        # A second push to the release branch: nothing to open any more.
        self.git("switch", "--quiet", "release/0.2.2")
        self.assertEqual(self.tool("release_flow.py", "open-next", "0.2.2",
                                   "--develop", "develop").stdout.strip(), "")
        self.assertEqual(self.git("branch", "--show-current").strip(), "release/0.2.2")

        self.release_branch_with_a_fix()
        self.assertEqual(self.back_merge(), "0.2.3")

        data, released = self.data(), self.data("main")
        self.assertEqual(data["current"], "0.2.3")
        self.assertEqual([r["version"] for r in data["releases"]], ["0.2.3", "0.2.2", "0.2.1"])
        self.assertEqual(vp.find_release(data, "0.2.2"), vp.find_release(released, "0.2.2"))
        self.assertEqual(vp.find_release(data, "0.2.3")["sections"][0]["groups"][0]["entries"],
                         ["A thing for 0.2.3."])
        self.assertEqual(self.tool("version.py", "check", "--base", "main").returncode, 0)

        # The merge commit has a clean message (no "# Conflicts:" lines).
        message = self.git("log", "-1", "--format=%B", "chore/back-merge-v0.2.2")
        self.assertTrue(message.startswith("chore(release): merge v0.2.2 back into develop"))
        self.assertNotIn("#", message)

        # The pull request into develop has no conflict.
        self.merge("chore/back-merge-v0.2.2", "develop")
        self.assertEqual(self.data("develop"), data)

    def test_back_merge_opens_the_next_version_without_a_cut(self) -> None:
        self.git("switch", "--quiet", "-c", "release/0.2.2")
        self.release_branch_with_a_fix()
        self.assertEqual(self.back_merge(), "0.2.3")
        data = self.data()
        self.assertEqual([r["version"] for r in data["releases"]], ["0.2.3", "0.2.2", "0.2.1"])
        self.assertEqual(vp.find_release(data, "0.2.2")["date"], "2026-10-30")

    def test_back_merge_is_done_when_develop_contains_the_release(self) -> None:
        self.git("switch", "--quiet", "-c", "release/0.2.2")
        self.release_branch_with_a_fix()
        self.back_merge()
        self.merge("chore/back-merge-v0.2.2", "develop")
        released = self.git("rev-parse", "main").strip()
        result = self.tool("release_flow.py", "back-merge", "0.2.2",
                           "--release", released, "--develop", "develop")
        self.assertEqual(result.stdout.strip(), "")

    def test_a_branch_older_than_the_cut_is_caught(self) -> None:
        self.git("switch", "--quiet", "-c", "feature/late", "develop")
        self.version("add", "Added", "A late thing, logged before the cut.")
        self.commit("feat: a late thing")

        self.git("switch", "--quiet", "-c", "release/0.2.2", "develop")
        self.tool("release_flow.py", "open-next", "0.2.2", "--develop", "develop")
        self.merge("chore/open-v0.2.3", "develop")
        develop = self.git("rev-parse", "develop").strip()

        # What CI checks on the pull request: its merge into develop.
        self.git("switch", "--quiet", "--detach", develop)
        merge = subprocess.run(["git", "merge", "--no-ff", "--no-edit", "feature/late"],
                               cwd=self.repo, capture_output=True, text=True)
        if merge.returncode != 0:   # only the generated file can conflict
            self.assertEqual(self.git("diff", "--name-only", "--diff-filter=U").split(),
                             ["version.md"])
            self.version("render")
            self.commit("merge")
        result = self.tool("version.py", "check", "--base", develop, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("v0.2.2 is in a release branch", result.stderr)

    def test_open_next_stops_when_develop_changed_the_release_after_the_cut(self) -> None:
        # A pull request whose CI ran before the cut is merged after it.
        self.git("switch", "--quiet", "-c", "feature/late", "develop")
        self.version("add", "Added", "A late thing, logged before the cut.")
        self.commit("feat: a late thing")
        self.git("switch", "--quiet", "-c", "release/0.2.2", "develop")
        self.merge("feature/late", "develop")

        self.git("switch", "--quiet", "release/0.2.2")
        result = self.tool("release_flow.py", "open-next", "0.2.2", "--develop", "develop",
                           check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("develop changed v0.2.2 after release/0.2.2 was cut", result.stderr)
        self.assertEqual(self.git("branch", "--show-current").strip(), "release/0.2.2")

    def test_open_next_needs_the_version_of_the_branch(self) -> None:
        self.git("switch", "--quiet", "-c", "release/0.2.3")
        result = self.tool("release_flow.py", "open-next", "0.2.3", "--develop", "develop",
                           check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("release/0.2.2", result.stderr)


if __name__ == "__main__":
    unittest.main()
