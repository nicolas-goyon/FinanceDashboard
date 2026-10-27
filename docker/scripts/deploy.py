#!/usr/bin/env python3
"""Package a FinanceDashboard build into a runnable folder (dist/<variant>/).

Called by build-app.sh inside the build container.

Why not windeployqt / Qt's CMake deploy API?
  windeployqt is a Windows executable and can't run in the Linux container
  when cross-compiling, so the shared builds are deployed by hand here, with
  the same layout windeployqt / a Qt prefix uses:

  Every variant: <out>/FinanceDashboard[.exe] is the *launcher*
  (launcher/windows, launcher/linux — checks requirements, then starts the
  app), and the real app lives in <out>/app/ with app/requirements.txt.

  windows-shared:  app/FinanceDashboard.exe + *.dll (exe dir — Windows' loader
                   looks there), app/plugins/<type>/*.dll, app/qml/<Module>/…,
                   app/qt.conf
  linux-shared:    app/FinanceDashboard (RUNPATH=$ORIGIN/lib), app/lib/*.so*,
                   app/plugins/<type>/*.so, app/qml/<Module>/…, app/qt.conf
                   — mirrors Qt's own prefix layout so the relative RUNPATHs
                   baked into Qt's plugins ($ORIGIN/../../lib, …) keep working.
  *-static:        app/FinanceDashboard[.exe] only (Qt + QML plugins linked in).

qt.conf keys/defaults: https://doc.qt.io/qt-6/qt-conf.html (Prefix defaults to
the executable's directory; Plugins → "plugins", QmlImports → "qml",
Libraries → "lib").

QML modules to ship come from qmlimportscanner (the same tool Qt's own build
uses for static QML plugin import), run over the app's source tree.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

APP = "FinanceDashboard"

# Qt plugin types/files to ship for shared builds. Plugin directories that
# don't exist in the Qt install are skipped silently. "tls": the HTTPS of the
# update check (#117) — Schannel on Windows (the TLS of the system, no OpenSSL
# DLL), OpenSSL on Linux (Qt loads the libssl.so.3 of the system at run time).
WINDOWS_PLUGINS = {
    "platforms": ["qwindows.dll"],
    "sqldrivers": ["qsqlite.dll"],
    "imageformats": None,  # None = whole directory
    "tls": ["qschannelbackend.dll"],
}
LINUX_PLUGINS = {
    "platforms": None,
    "platforminputcontexts": None,
    "xcbglintegrations": None,
    "wayland-shell-integration": None,
    "wayland-graphics-integration-client": None,
    "wayland-decoration-client": None,
    "platformthemes": ["libqxdgdesktopportal.so"],  # gtk3 theme would drag in GTK
    "sqldrivers": ["libqsqlite.so"],
    "imageformats": None,
    "tls": ["libqopensslbackend.so"],
}
SKIP_SUFFIXES = (".debug", ".pdb", ".prl", ".a", ".lib")
# Plugin types whose listed files must all be in the Qt install: a missing
# one stops the deploy, instead of a package that fails when it runs. "tls":
# without it, the update check (#117, HTTPS) cannot work.
REQUIRED_PLUGIN_TYPES = {"tls"}


def log(msg: str) -> None:
    print(f"[deploy] {msg}", flush=True)


def run(cmd: list[str]) -> str:
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def find_executable(build_dir: Path, target: str) -> Path:
    name = APP + (".exe" if target == "windows" else "")
    direct = build_dir / name
    if direct.is_file():
        return direct
    hits = [p for p in build_dir.rglob(name) if p.is_file()]
    if not hits:
        sys.exit(f"[deploy] error: {name} not found under {build_dir}")
    return hits[0]


def copy_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst, follow_symlinks=True)


# ── QML modules ──────────────────────────────────────────────────────────────
def find_tool(name: str, *qt_prefixes: Path) -> Path:
    for prefix in qt_prefixes:
        for sub in ("libexec", "bin"):
            p = prefix / sub / name
            if p.is_file():
                return p
    sys.exit(f"[deploy] error: {name} not found in {', '.join(map(str, qt_prefixes))}")


def qml_module_dirs(src_dir: Path, qt_target: Path, qt_host: Path) -> list[Path]:
    """Directories (inside qt_target/qml) of every QML module the app imports,
    including transitive dependencies, as resolved by qmlimportscanner."""
    scanner = find_tool("qmlimportscanner", qt_host, qt_target)
    qml_root = qt_target / "qml"
    out = run([str(scanner), "-rootPath", str(src_dir), "-importPath", str(qml_root)])
    dirs: set[Path] = set()
    for entry in json.loads(out):
        path = entry.get("path")
        if entry.get("type") != "module" or not path:
            continue
        p = Path(path).resolve()
        if qml_root.resolve() in p.parents:
            dirs.add(p)
    return sorted(dirs)


def copy_qml_module(module_dir: Path, qml_root: Path, out_qml: Path) -> list[Path]:
    """Copy one QML module directory. Sub-directories that are themselves QML
    modules (have their own qmldir) are *not* copied — they're shipped only if
    qmlimportscanner listed them. Plain sub-directories (images, helpers) are
    copied recursively. Returns the copied binaries (plugins) for dependency
    resolution."""
    binaries: list[Path] = []
    rel = module_dir.relative_to(qml_root.resolve())

    def walk(d: Path, dest: Path) -> None:
        for item in d.iterdir():
            if item.is_dir():
                if (item / "qmldir").exists():
                    continue  # separate module
                walk(item, dest / item.name)
            elif item.is_file() and not item.name.endswith(SKIP_SUFFIXES):
                copy_file(item, dest / item.name)
                if item.suffix in (".dll", ".so") or ".so." in item.name:
                    binaries.append(item)

    walk(module_dir, out_qml / rel)
    return binaries


# ── Plugins ──────────────────────────────────────────────────────────────────
def copy_plugins(qt_target: Path, out_plugins: Path, spec: dict) -> list[Path]:
    binaries: list[Path] = []
    for ptype, names in spec.items():
        src = qt_target / "plugins" / ptype
        if ptype in REQUIRED_PLUGIN_TYPES:
            missing = [n for n in (names or []) if not (src / n).is_file()]
            if not src.is_dir() or missing:
                sys.exit(f"[deploy] error: {', '.join(missing) or 'the ' + ptype + ' plugins'} "
                         f"not found in {src} — the update check (HTTPS) needs them")
        if not src.is_dir():
            continue
        candidates = [src / n for n in names] if names else sorted(src.iterdir())
        for f in candidates:
            if f.is_file() and not f.name.endswith(SKIP_SUFFIXES):
                copy_file(f, out_plugins / ptype / f.name)
                binaries.append(f)
    return binaries


# ── Shared-library dependency resolution ────────────────────────────────────
def pe_imports(binary: Path) -> list[str]:
    out = run(["llvm-objdump", "-p", str(binary)])
    return re.findall(r"DLL Name:\s*(\S+)", out)


def deploy_windows_dlls(roots: list[Path], search_dirs: list[Path], out_dir: Path) -> None:
    """Recursively copy every non-system DLL the given binaries import.
    Anything not found in search_dirs (KERNEL32, d3d11, api-ms-win-*, …) is
    assumed to be a Windows system DLL."""
    index: dict[str, Path] = {}
    for d in search_dirs:
        if d.is_dir():
            for f in d.glob("*.dll"):
                index.setdefault(f.name.lower(), f)
    seen: set[str] = set()
    queue = list(roots)
    while queue:
        for name in pe_imports(queue.pop()):
            key = name.lower()
            if key in seen:
                continue
            seen.add(key)
            src = index.get(key)
            if src is None:
                if key.startswith(("qt6", "libc++", "libunwind", "libwinpthread", "libgcc", "libstdc++")):
                    log(f"WARNING: {name} is needed but was not found — the app will not start")
                continue
            copy_file(src, out_dir / src.name)
            queue.append(src)


def deploy_linux_libs(roots: list[Path], qt_target: Path, out_lib: Path) -> None:
    """Copy every shared library the binaries need that lives in Qt's lib dir
    (Qt itself + the ICU it bundles). System libraries (glibc, X11/xcb, GL,
    fontconfig, …) are expected on the machine that runs the app."""
    qt_lib = (qt_target / "lib").resolve()
    copied: set[str] = set()
    for binary in roots:
        out = subprocess.run(["ldd", str(binary)], capture_output=True, text=True).stdout
        for line in out.splitlines():
            m = re.match(r"\s*(\S+)\s+=>\s+(\S+)", line)
            if not m:
                continue
            soname, path = m.groups()
            if path == "not":
                if soname.startswith("libQt6"):
                    log(f"WARNING: {binary.name} needs {soname}, which was not found")
                continue
            p = Path(path)
            if qt_lib in p.resolve().parents and soname not in copied:
                copied.add(soname)
                copy_file(p, out_lib / soname)  # keep the SONAME as file name


def write_qt_conf(out_dir: Path) -> None:
    (out_dir / "qt.conf").write_text(
        "[Paths]\nPrefix = .\nPlugins = plugins\nQmlImports = qml\nLibraries = lib\n",
        encoding="utf-8",
    )


# ── Static: report what the exe still depends on ────────────────────────────
def report_static(exe: Path, target: str) -> None:
    if target == "windows":
        deps = pe_imports(exe)
        non_system = [d for d in deps if d.lower().startswith(("qt6", "lib"))]
        log("imports: " + ", ".join(deps))
        if non_system:
            log("WARNING: not fully static, still imports " + ", ".join(non_system))
        else:
            log("OK: only Windows system DLLs are imported")
    else:
        out = subprocess.run(["ldd", str(exe)], capture_output=True, text=True).stdout
        qt = [l.strip() for l in out.splitlines() if "libQt6" in l]
        log("system libraries still needed at run time (normal for Linux):")
        print(out, flush=True)
        if qt:
            log("WARNING: Qt libraries are still linked dynamically: " + "; ".join(qt))


def find_launcher(build_dir: Path) -> Path:
    """The Windows launcher built by CMake (target FinanceDashboardLauncher)."""
    name = "FinanceDashboardLauncher.exe"
    direct = build_dir / name
    if direct.is_file():
        return direct
    hits = [p for p in build_dir.rglob(name) if p.is_file()]
    if not hits:
        sys.exit(f"[deploy] error: {name} not found under {build_dir}")
    return hits[0]


def write_requirements(app_dir: Path, target: str, link_mode: str, lib_check_targets: list[str]) -> int:
    """app/requirements.txt — read by the launcher before it starts the app
    (launcher/windows/main.cpp, launcher/linux/FinanceDashboard.sh). Lists every
    packaged file, so an incomplete copy of the folder is caught with a clear
    message instead of a loader error."""
    files = sorted(
        p.relative_to(app_dir).as_posix()
        for p in app_dir.rglob("*")
        if p.is_file() and p.name != "requirements.txt"
    )
    lines = [
        "# Generated by docker/scripts/deploy.py — read by the FinanceDashboard launcher.",
        "# Format: one 'key value' per line. See docs/startup.md.",
        f"target {target}",
        f"link-mode {link_mode}",
    ]
    if target == "windows":
        # Qt 6.11: Windows 10 1809 (build 17763) or later —
        # https://doc.qt.io/qt-6/supported-platforms.html
        lines.append("min-windows-build 17763")
    lines += [f"check-libs {t}" for t in lib_check_targets]
    lines += [f"file {f}" for f in files]
    (app_dir / "requirements.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return len(files)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", choices=["windows", "linux"], required=True)
    ap.add_argument("--link-mode", choices=["shared", "static"], required=True)
    ap.add_argument("--build-dir", type=Path, required=True)
    ap.add_argument("--src-dir", type=Path, required=True)
    ap.add_argument("--out-dir", type=Path, required=True)
    ap.add_argument("--qt-target", type=Path, required=True)
    ap.add_argument("--qt-host", type=Path, required=True)
    a = ap.parse_args()

    # Layout: <out>/FinanceDashboard[.exe] = launcher, <out>/app/ = the real app.
    exe = find_executable(a.build_dir, a.target)
    if a.out_dir.exists():
        shutil.rmtree(a.out_dir)
    app_dir = a.out_dir / "app"
    app_dir.mkdir(parents=True)
    out_exe = app_dir / exe.name
    copy_file(exe, out_exe)
    log(f"{exe.name} → {app_dir}")

    if a.target == "windows":
        launcher = find_launcher(a.build_dir)
        copy_file(launcher, a.out_dir / f"{APP}.exe")
        log(f"launcher {launcher.name} → {a.out_dir / (APP + '.exe')}")
    else:
        dst = a.out_dir / APP
        copy_file(a.src_dir / "launcher" / "linux" / "FinanceDashboard.sh", dst)
        dst.chmod(0o755)
        log(f"launcher → {dst}")

    lib_check_targets = [exe.name]
    if a.link_mode == "static":
        report_static(out_exe, a.target)
    else:
        qml_root = a.qt_target / "qml"
        binaries: list[Path] = [exe]
        modules = qml_module_dirs(a.src_dir, a.qt_target, a.qt_host)
        for mod in modules:
            binaries += copy_qml_module(mod, qml_root, app_dir / "qml")
        log(f"{len(modules)} QML module(s): " + ", ".join(
            str(m.relative_to(qml_root.resolve())) for m in modules))

        spec = WINDOWS_PLUGINS if a.target == "windows" else LINUX_PLUGINS
        binaries += copy_plugins(a.qt_target, app_dir / "plugins", spec)

        if a.target == "windows":
            deploy_windows_dlls(
                binaries,
                [a.qt_target / "bin", Path("/opt/llvm-mingw/x86_64-w64-mingw32/bin")],
                app_dir,
            )
        else:
            deploy_linux_libs(binaries, a.qt_target, app_dir / "lib")
            subprocess.run(["patchelf", "--set-rpath", "$ORIGIN/lib", str(out_exe)], check=True)
            # The xcb platform plugin is where most missing system libraries
            # (libxcb-cursor0, xkbcommon-x11, …) show up.
            lib_check_targets += [
                p.relative_to(app_dir).as_posix()
                for p in sorted((app_dir / "plugins" / "platforms").glob("libqxcb*.so"))
            ]
        write_qt_conf(app_dir)

    if a.target == "windows":
        lib_check_targets = []  # the launcher only checks files + OS on Windows
    n = write_requirements(app_dir, a.target, a.link_mode, lib_check_targets)
    log(f"packaged {n} files (+ launcher, requirements.txt)")


if __name__ == "__main__":
    main()
