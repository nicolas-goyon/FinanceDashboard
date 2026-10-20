# Contributing

This project follows gitflow. This page tells how a change goes from an issue to a release.
For how CI builds, tests and publishes the app, see `CI-CD.md`.

---

## Branches

| Branch | Role | Created from | Merges into |
|---|---|---|---|
| `main` | Released code only. Each merge is tagged `vX.Y.Z`. | — | — |
| `develop` | Integration branch, and the default target of pull requests. | — | — |
| `feature/<issue>-<slug>` | New work, for example `feature/42-sankey-balance`. | `develop` | `develop` (pull request, squash merge) |
| `bugfix/<issue>-<slug>` | A fix for something that is not released yet. | `develop`, or `release/<x.y.z>` for a fix of that release | the branch it was created from (pull request, squash merge) |
| `release/<x.y.z>` | Prepares a release: the last fixes and the date of the version. | `develop` | `main` (merge commit), then back into `develop` |
| `hotfix/<x.y.z>` | An urgent fix of a release. | `main` | `main` (merge commit), then back into `develop` |

Do not push to `main` or `develop` directly.

**History rewrite.** On 2026-10-20 at 06:39 UTC, the history on GitHub was rewritten to remove
personal data. Delete a clone made before that time and clone the repository again. Do not push
from an old clone: it would bring the old history back.

---

## From an issue to a merge

1. Each change starts from an issue, with its labels (see [Labels](#labels)).
2. Create the branch from `develop`, for example `git switch -c bugfix/16-null-strings develop`.
3. Commit with Conventional Commits (see [Commit messages](#commit-messages)).
4. Open a pull request into `develop`, with `Closes #<issue>` in its description. The template
   lists what the pull request needs.
5. CI must be green. The maintainer reviews the pull request and merges it.

**Batches.** Put related changes in one pull request: a general refactor is one pull request.
Changes in different places can be a batch of several pull requests.

**Stacked pull requests.** The pull requests of a batch, and the batches themselves, form a
stack: each pull request is based on the branch of the one before it, so its diff shows only its
own changes, and CI checks it with everything below it. Merge from the bottom up. After a merge
into `develop`, the next pull request targets `develop` again: GitHub does it when the merged
branch is deleted; otherwise rebase it with
`git rebase --onto develop <merged-branch> <next-branch>`. A change to a pull request in the
middle of the stack means a rebase of every pull request above it. The usual conflict is
`version.json`: resolve it there, then run `python tools/version.py render`.

**Merging a stack.** A whole stack can be merged in one operation: on GitHub, merge the highest
pull request to merge. GitHub merges it together with all the unmerged pull requests below it
([merging stacked pull requests](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/merging-stacked-pull-requests),
public preview). `develop` then gets one push and one CI run for all of them. Do it only when CI
is green on each pull request of the stack: the CI run of each pull request tested it with all
the pull requests below it.

---

## Commit messages

[Conventional Commits](https://www.conventionalcommits.org/): `<type>(<scope>): <summary>`.
Write the summary in the imperative, without a final period.

- Types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `ci`, `build`, `chore`.
- Scopes (optional): `import`, `db`, `ui`, `charts`, `predictor`, `ci`, `packaging`, `docker`,
  `repo`.
- Example: `fix(import): bind empty strings, not NULL (#16)`.
- In the body, tell why the change is necessary. Add `Closes #<issue>` when the commit closes
  an issue.

---

## Labels

| Group | Labels | Use |
|---|---|---|
| `type:` | `bug`, `feature`, `chore`, `docs`, `test`, `refactor`, `security` | One per issue. |
| `priority:` | `P0` (data loss or corruption, or the release is blocked), `P1` (a feature is broken or gives a wrong result), `P2` (edge case, robustness), `P3` (cosmetic, cleanup) | One per issue. |
| `area:` | `data`, `import`, `predictor`, `ui`, `charts`, `ci`, `packaging`, `docker`, `repo`, `tooling` | One or more. |
| `status:` | `needs decision`, `blocked` | When the issue waits for the maintainer or for another issue. |

Each milestone is one release (`vX.Y.Z`).

---

## Definition of done

A pull request is ready when:

- CI is green.
- New behaviour has a test (`ctest`). A fixed bug has a regression test when it can be tested.
- The changelog has the change: `python tools/version.py add <Section> "<text>"`. For a fix to a
  feature that is not released yet, change the entry of that feature instead of adding a line.
- `README.md` and `ARCHITECTURE.MD` describe the new behaviour or structure.

---

## Releases

See `CI-CD.md` § The release flow. In short:

1. Cut `release/<x.y.z>` from `develop` and push it. CI opens a pull request into `develop` that
   opens the next version. Merge it before any other pull request into `develop`. Then update
   the branch of each open pull request into `develop`, so that its CI runs again.
2. Fixes for the release go into the release branch: a `bugfix/*` branch from it, and a pull
   request into it.
3. When the release is ready, run `python tools/version.py release` on the release branch (it
   writes the date of the release), then merge the release branch into `main` with a merge
   commit.
4. CI publishes the release. Then it opens a pull request that merges `main` back into
   `develop`, and starts CI on it. Merge it with a merge commit, not a squash: `develop` must
   contain the release commit.

---

## Repository settings

These settings are not in a file. The maintainer sets them once:

- Settings → General → Default branch: `develop`.
- Settings → General → Pull Requests: allow squash merging and merge commits, and turn on
  "Automatically delete head branches".
- Settings → Actions → General → Workflow permissions: "Allow GitHub Actions to create and
  approve pull requests". The release job needs it to open the back-merge pull request.
- Branch rules for `main` and `develop` (pull requests only, CI green before merge). On a
  private repository, rulesets need GitHub Pro or Team. Without them, follow the rule by
  convention. The back-merge pull request can pass the rule too: the release job starts its CI
  run.
