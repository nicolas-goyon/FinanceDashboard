---

### This is a pre-release of the Test app

**Finance Dashboard Test** is a second app for the experimental and preprod
builds. It installs and runs next to Finance Dashboard, with its own data
folder, so a pre-release never touches the data of the prod app. Its icon has
an orange "TEST" band, and its window title ends with "(Test)". To try a
pre-release on your real data, use **Copy my prod database** on its Database
Management page: the prod database is only read.

### Downloads

| Platform | File | What it is |
|---|---|---|
| **Windows** | `FinanceDashboardTest-<version>-windows-x64.exe` | Setup of the Test app. To update, run the setup of a newer pre-release. |
| Windows | `FinanceDashboardTest-<version>-windows-x64.zip` | Portable. Unzip anywhere and run `FinanceDashboard.exe`. |
| **Linux** | `FinanceDashboardTest-<version>-linux-x86_64.AppImage` | Single file. `chmod +x` it, run it. |

A pre-release has no `.deb`, no `.tar.gz` and no source archive: those come
with the releases of Finance Dashboard.

The Linux build is made on Ubuntu 24.04, so it needs glibc 2.39 or newer
(Ubuntu 24.04+, Debian 13+, Fedora 40+).

### Verifying a download

Every file's SHA-256 is listed in `SHA256SUMS.txt`:

```sh
sha256sum -c SHA256SUMS.txt --ignore-missing
```

### The data of the Test app

- Windows — `%APPDATA%\DataAnalysis\FinanceDashboardTest\finance_dashboard.db`
- Linux — `~/.local/share/DataAnalysis/FinanceDashboardTest/finance_dashboard.db`

A newer pre-release can upgrade this database. An older version then refuses
it: put back the copy that the upgrade saved next to it
(`finance_dashboard.backup-<date>.db`), or copy the prod database again.
