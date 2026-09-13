---

### Downloads

Pick one file. Nothing here needs Qt, a compiler, or anything else installed
first — the runtime travels inside every package.

| Platform | File | What it is |
|---|---|---|
| **Windows** | `FinanceDashboard-<version>-windows-x64.msi` | Installer. Start-menu and desktop shortcuts, proper uninstaller, upgrades in place over an older version. |
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
Get-FileHash .\FinanceDashboard-*.msi -Algorithm SHA256
```

### Your data

Finance Dashboard keeps everything in a local SQLite file and talks to no
network service of any kind:

- Windows — `%APPDATA%\DataAnalysis\FinanceDashboard\finance_dashboard.db`
- Linux — `~/.local/share/DataAnalysis/FinanceDashboard/finance_dashboard.db`

Uninstalling does not delete it. Back that one file up and you have backed up
everything.
