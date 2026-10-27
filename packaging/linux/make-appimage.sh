#!/usr/bin/env bash
# ── make-appimage.sh ──────────────────────────────────────────────────────────
# Turn a `cmake --install` tree into a single, self-contained .AppImage: the
# "download one file, chmod +x, double-click" format for Linux. No Qt, no
# libraries and no package manager needed on the user's machine.
#
#   ./packaging/linux/make-appimage.sh <install-tree> <version> [output-dir]
#
#   <install-tree>  a prefix produced by `cmake --install build --prefix <dir>`,
#                   i.e. containing bin/FinanceDashboard, lib/, plugins/, qml/
#                   and share/ — see the install() rules in CMakeLists.txt.
#   <version>       X.Y.Z (from version.json), or X.Y.Z-exp.<run> / -rc.<run> for a
#                   pre-release of the Test app (#129). Used in the output file
#                   name and in the AppStream release.
#   [output-dir]    where to write the .AppImage (default: dist/)
#
# The install tree has the desktop entry of its identity (#130):
# finance-dashboard.desktop for the prod app, finance-dashboard-test.desktop
# for the Test app (cmake -DFD_APP_IDENTITY=test). The AppImage takes its
# desktop entry, icon, AppStream data and file name from that identity:
# FinanceDashboard-<version>-... or FinanceDashboardTest-<version>-...
#
# Why not linuxdeploy-plugin-qt: Qt's own CMake deployment API already bundled
# every library, plugin and QML module and rewrote the RPATHs when the install
# tree was made (https://doc.qt.io/qt-6/cmake-deployment.html). Running a second
# Qt bundler on top of that duplicates the work and is the usual source of
# "module X is not installed" failures at runtime. All that is left to do here
# is the AppDir wrapping, which is what this script does.
#
# Needs: appimagetool (downloaded automatically if not on PATH), and network
# access for that download.
set -euo pipefail

usage() { sed -n '2,25p' "$0" >&2; exit 2; }

INSTALL_TREE="${1:-}"
VERSION="${2:-}"
OUT_DIR="${3:-dist}"
[[ -n "$INSTALL_TREE" && -n "$VERSION" ]] || usage
[[ -x "$INSTALL_TREE/bin/FinanceDashboard" ]] || {
    echo "error: $INSTALL_TREE/bin/FinanceDashboard not found or not executable" >&2
    echo "       (did you run \`cmake --install <build> --prefix $INSTALL_TREE\`?)" >&2
    exit 1
}

# The identity of the install tree (#130).
if [[ -f "$INSTALL_TREE/share/applications/finance-dashboard-test.desktop" ]]; then
    APP_NAME=FinanceDashboardTest
    FILE_ID=finance-dashboard-test
    METAINFO_ID=io.github.nicolas_goyon.FinanceDashboardTest
else
    APP_NAME=FinanceDashboard
    FILE_ID=finance-dashboard
    METAINFO_ID=io.github.nicolas_goyon.FinanceDashboard
fi
for f in "share/applications/$FILE_ID.desktop" "share/icons/hicolor/256x256/apps/$FILE_ID.png"; do
    [[ -f "$INSTALL_TREE/$f" ]] || { echo "error: $INSTALL_TREE/$f not found" >&2; exit 1; }
done
echo "==> Identity: $APP_NAME"

ARCH="${ARCH:-$(uname -m)}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

APPDIR="$WORK/$APP_NAME.AppDir"
mkdir -p "$APPDIR"

echo "==> Copying install tree into AppDir/usr"
mkdir -p "$APPDIR/usr"
cp -a "$INSTALL_TREE/." "$APPDIR/usr/"

# ── AppDir metadata ───────────────────────────────────────────────────────────
# appimagetool wants the .desktop file and the icon at the AppDir root, on top
# of the FHS copies under usr/share that the .deb and desktop environments use.
echo "==> Writing AppDir metadata"
install -m 644 "$INSTALL_TREE/share/applications/$FILE_ID.desktop" "$APPDIR/$FILE_ID.desktop"
install -m 644 "$INSTALL_TREE/share/icons/hicolor/256x256/apps/$FILE_ID.png" "$APPDIR/$FILE_ID.png"
cp "$APPDIR/$FILE_ID.png" "$APPDIR/.DirIcon"

# Stamp the release into the AppStream metadata so software centres show a
# version and a date for this build instead of "unknown".
METAINFO="$APPDIR/usr/share/metainfo/$METAINFO_ID.metainfo.xml"
if [[ -f "$METAINFO" ]]; then
    python3 - "$METAINFO" "$VERSION" <<'PY'
import sys, datetime, pathlib
path, version = pathlib.Path(sys.argv[1]), sys.argv[2]
today = datetime.date.today().isoformat()
text = path.read_text(encoding="utf-8")
release = f'<releases>\n    <release version="{version}" date="{today}"/>\n  </releases>'
path.write_text(text.replace("<releases/>", release), encoding="utf-8")
PY
fi

# ── Vendor the X11/xcb helper libraries Qt's platform plugin needs ────────────
# qt_deploy_runtime_dependencies deliberately skips anything living in a system
# library directory, which is right for glibc, libGL and libX11 (bundling those
# breaks more than it fixes) but wrong for the small xcb helpers: they are the
# classic "works here, fails on the user's machine with
# 'could not load the Qt platform plugin xcb'" dependency, and plenty of
# desktop installs simply do not have libxcb-cursor. So copy the short
# allowlist below in, the same way linuxdeploy does.
echo "==> Vendoring xcb helper libraries"
VENDOR_LIBS=(
    libxcb-cursor.so.0 libxcb-icccm.so.4 libxcb-image.so.0 libxcb-keysyms.so.1
    libxcb-randr.so.0 libxcb-render-util.so.0 libxcb-shape.so.0 libxcb-util.so.1
    libxcb-xinerama.so.0 libxcb-xkb.so.1
    libxkbcommon.so.0 libxkbcommon-x11.so.0
)
mkdir -p "$APPDIR/usr/lib"
for lib in "${VENDOR_LIBS[@]}"; do
    if [[ -e "$APPDIR/usr/lib/$lib" ]]; then continue; fi
    src="$(ldconfig -p 2>/dev/null | awk -v n="$lib" '$1 == n { print $NF; exit }')"
    if [[ -n "$src" && -e "$src" ]]; then
        cp -L "$src" "$APPDIR/usr/lib/$lib"
        echo "    + $lib"
    else
        echo "    - $lib (not found on this builder, skipped)"
    fi
done

# ── AppRun ────────────────────────────────────────────────────────────────────
# The install tree already carries bin/qt.conf and an $ORIGIN/../lib RPATH, so
# the binary would find its own libraries even without this. The exports are
# deliberate belt-and-braces: an AppImage runs on distributions this build never
# saw, and a stray QT_PLUGIN_PATH or QML_IMPORT_PATH in the user's environment
# would otherwise send Qt looking at the host's Qt instead of the bundled one.
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QML_IMPORT_PATH="$HERE/usr/qml"
export QML2_IMPORT_PATH="$HERE/usr/qml"
export XDG_DATA_DIRS="$HERE/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
exec "$HERE/usr/bin/FinanceDashboard" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

# ── appimagetool ──────────────────────────────────────────────────────────────
APPIMAGETOOL="$(command -v appimagetool || true)"
if [[ -z "$APPIMAGETOOL" ]]; then
    echo "==> Downloading appimagetool"
    APPIMAGETOOL="$WORK/appimagetool"
    curl -fsSL -o "$APPIMAGETOOL" \
        "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage"
    chmod +x "$APPIMAGETOOL"
fi

mkdir -p "$OUT_DIR"
OUTPUT="$OUT_DIR/$APP_NAME-${VERSION}-linux-${ARCH}.AppImage"

echo "==> Building $OUTPUT"
# --appimage-extract-and-run: CI runners have no FUSE, so appimagetool (itself
# an AppImage) cannot mount itself and must unpack instead.
# --no-appstream: appstreamcli is not installed on a bare runner; the metadata
# is still shipped, just not validated here.
ARCH="$ARCH" "$APPIMAGETOOL" --appimage-extract-and-run --no-appstream \
    "$APPDIR" "$OUTPUT"

chmod +x "$OUTPUT"
echo "==> Done: $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
