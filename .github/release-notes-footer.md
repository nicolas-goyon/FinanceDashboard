---

### Downloads

Pick one file. Nothing here needs Qt, a compiler, or anything else installed
first — the runtime travels inside every package.

| Platform | File | What it is |
|---|---|---|
| **Windows** | `FinanceDashboard-<version>-windows-x64.exe` | Setup. Installs for you only or for all users, with a Start-menu shortcut, an optional desktop shortcut and an uninstaller. To update, run the setup of a newer version: it updates the installed copy in place. |
| Windows | `FinanceDashboard-<version>-windows-x64.zip` | Portable. Unzip anywhere and run `FinanceDashboard.exe`. Nothing is written outside the folder except the database. |
| **Linux** | `FinanceDashboard-<version>-linux-x86_64.AppImage` | Single file. `chmod +x` it, run it. Works across distributions. |
| Linux | `finance-dashboard_<version>_amd64.deb` | Debian/Ubuntu: `sudo apt install ./finance-dashboard_<version>_amd64.deb`. Installs under `/opt`, adds a menu entry. |
| Linux | `FinanceDashboard-<version>-linux-x86_64.tar.gz` | Portable. Extract and run `bin/FinanceDashboard`. |
| Source | `FinanceDashboard-<version>-source.tar.gz` / `.zip` | Exactly the tree this release was built from. |

Linux builds are produced on Ubuntu 24.04, so they need glibc 2.39 or newer
(Ubuntu 24.04+, Debian 13+, Fedora 40+). On older distributions, build from
source — see `README.md`.

### Verifying a download

Every file's SHA-256 is listed in `SHA256SUMS.txt`:

```sh
sha256sum -c SHA256SUMS.txt --ignore-missing
```

```powershell
Get-FileHash .\FinanceDashboard-*-windows-x64.* -Algorithm SHA256
```

### Your data

Finance Dashboard keeps everything in a local SQLite file. Your data never
leaves your computer:

- Windows — `%APPDATA%\DataAnalysis\FinanceDashboard\finance_dashboard.db`
- Linux — `~/.local/share/DataAnalysis/FinanceDashboard/finance_dashboard.db`

When a new version changes the database format, the app first saves a copy
next to it (`finance_dashboard.backup-<date>.db`) and tells you. If the upgrade
fails, the app does not start, tells you why, and leaves the file as it was.

Uninstalling does not delete the database. Back that one file up and you have
backed up everything.

### The update check

The app connects to one network service only: at each start, it asks GitHub
for the list of releases, to tell you when a new version exists. GitHub sees
the internet address of your computer and the version of the app; the app
sends nothing else. Turn the check off on the Updates page. On Linux, the check
uses the OpenSSL 3 of the system (`libssl.so.3`).
