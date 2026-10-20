#!/usr/bin/env bash
# ── make-deb.sh ───────────────────────────────────────────────────────────────
# Turn a `cmake --install` tree into a self-contained .deb for Debian/Ubuntu
# users who would rather `apt install ./file.deb` than keep an AppImage around.
#
#   ./packaging/linux/make-deb.sh <install-tree> <version> [output-dir]
#
# Layout, deliberately the "vendored app" one rather than the distro one:
#   /opt/FinanceDashboard/                 the whole install tree, Qt included
#   /usr/bin/finance-dashboard             symlink to the binary
#   /usr/share/applications/*.desktop      menu entry
#   /usr/share/icons/hicolor/256x256/...   icon
#   /usr/share/metainfo/*.xml              AppStream metadata
#
# The package therefore Depends on no Qt package: the Qt libraries it needs
# travel inside it, exactly like the AppImage and the Windows installer. That
# is the point — the user asked for plug and play, not for a package that
# pulls half of Qt from the distribution's repositories and then disagrees with
# it about versions. It is *not* how a package accepted into Debian proper
# would be built; if this ever goes to a real repository, that version links
# against the distro's Qt instead.
#
# Qt's deployment leaves the system libraries out (glibc, X11/xcb, Wayland,
# OpenGL/EGL, fonts, D-Bus, GLib), so Depends must list them. This script reads
# the NEEDED entries of the files that the app loads at start-up (the binary,
# the bundled libraries, the platform plugins and their integrations, the input
# contexts, the image formats, the SQLite driver and the QML plugins), keeps the
# libraries that are not bundled, and asks dpkg which package installed each
# one on the build machine. Build on the oldest system the package is for (CI:
# Ubuntu 24.04). Optional plugins (other SQL drivers, the GTK theme, eglfs) are
# left out: Qt skips a plugin whose libraries are missing. The TLS backend of
# the update check (#117) loads OpenSSL at run time, with no NEEDED entry: its
# packages are added by name below.
# packaging/linux/test-deb.sh checks the result in a clean container.
#
# Why not CPack's DEB generator: it installs under a single prefix, so the
# /opt payload plus the four /usr desktop-integration files would need two CPack
# runs and a component split for what is twenty readable lines of shell here.
#
# Needs: dpkg-deb, dpkg and objdump (binutils) — Debian/Ubuntu; fakeroot optional.
set -euo pipefail

usage() { sed -n '2,30p' "$0" >&2; exit 2; }

INSTALL_TREE="${1:-}"
VERSION="${2:-}"
OUT_DIR="${3:-dist}"
[[ -n "$INSTALL_TREE" && -n "$VERSION" ]] || usage
# The .deb is for the prod app only (#130): the Test app is an AppImage.
if [[ -f "$INSTALL_TREE/share/applications/finance-dashboard-test.desktop" ]]; then
    echo "error: $INSTALL_TREE is a Test app build (FD_APP_IDENTITY=test); the .deb is for the prod app only" >&2
    exit 1
fi
[[ -x "$INSTALL_TREE/bin/FinanceDashboard" ]] || {
    echo "error: $INSTALL_TREE/bin/FinanceDashboard not found or not executable" >&2
    exit 1
}
command -v dpkg-deb >/dev/null || { echo "error: dpkg-deb not found" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

case "$(uname -m)" in
    x86_64)  DEB_ARCH=amd64 ;;
    aarch64) DEB_ARCH=arm64 ;;
    *)       DEB_ARCH="$(dpkg --print-architecture)" ;;
esac

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
ROOT="$WORK/pkg"

echo "==> Staging package tree"
mkdir -p "$ROOT/opt/FinanceDashboard" \
         "$ROOT/usr/bin" \
         "$ROOT/usr/share/applications" \
         "$ROOT/usr/share/icons/hicolor/256x256/apps" \
         "$ROOT/usr/share/metainfo" \
         "$ROOT/DEBIAN"

cp -a "$INSTALL_TREE/." "$ROOT/opt/FinanceDashboard/"
# The desktop-integration copies live in /usr, not twice under /opt.
rm -rf "$ROOT/opt/FinanceDashboard/share/applications" \
       "$ROOT/opt/FinanceDashboard/share/icons" \
       "$ROOT/opt/FinanceDashboard/share/metainfo"
rmdir --ignore-fail-on-non-empty "$ROOT/opt/FinanceDashboard/share" 2>/dev/null || true

ln -s /opt/FinanceDashboard/bin/FinanceDashboard "$ROOT/usr/bin/finance-dashboard"

install -m 644 "$REPO_ROOT/packaging/linux/finance-dashboard.desktop" \
               "$ROOT/usr/share/applications/finance-dashboard.desktop"
# Exec= must be an absolute path (or a name on PATH) for an installed package.
sed -i 's|^Exec=FinanceDashboard|Exec=/opt/FinanceDashboard/bin/FinanceDashboard|' \
       "$ROOT/usr/share/applications/finance-dashboard.desktop"

install -m 644 "$REPO_ROOT/packaging/icons/finance-dashboard-256.png" \
               "$ROOT/usr/share/icons/hicolor/256x256/apps/finance-dashboard.png"
install -m 644 "$REPO_ROOT/packaging/linux/io.github.nicolas_goyon.FinanceDashboard.metainfo.xml" \
               "$ROOT/usr/share/metainfo/io.github.nicolas_goyon.FinanceDashboard.metainfo.xml"

INSTALLED_SIZE_KB="$(du -sk "$ROOT" | cut -f1)"

echo "==> Finding the system libraries that the app needs"
command -v objdump >/dev/null || { echo "error: objdump not found (binutils)" >&2; exit 1; }
APP_ROOT="$ROOT/opt/FinanceDashboard"
mapfile -d '' ELF_FILES < <(find "$APP_ROOT/bin" "$APP_ROOT/lib" "$APP_ROOT/qml" \
        "$APP_ROOT/plugins/platforms" "$APP_ROOT/plugins/xcbglintegrations" \
        "$APP_ROOT/plugins/platforminputcontexts" "$APP_ROOT/plugins/imageformats" \
        "$APP_ROOT/plugins"/wayland-* "$APP_ROOT/plugins/sqldrivers/libqsqlite.so" \
        "$APP_ROOT/plugins/tls" \
        -type f \( -name '*.so*' -o -name FinanceDashboard \) -print0 2>/dev/null || true)
declare -A DEP_PACKAGES=()
# Read once. awk reads all its input below (no early `exit`, no `head`): under
# `set -o pipefail`, a reader that stops early makes the writer fail with
# SIGPIPE, and the script would stop with no message.
LDCONFIG_CACHE="$(ldconfig -p)"
for elf in "${ELF_FILES[@]}"; do
    while read -r lib; do
        [ -e "$APP_ROOT/lib/$lib" ] && continue          # bundled
        path="$(awk -v l="$lib" '$1 == l && /x86-64|AArch64/ && path == "" {path = $NF} END {print path}' \
                <<<"$LDCONFIG_CACHE")"
        if [ -z "$path" ]; then
            echo "error: ${elf#"$APP_ROOT/"} needs $lib, which is not on this machine" >&2
            exit 1
        fi
        # dpkg -S fails when no package owns the file: `|| true`, so that the
        # error message below is printed instead of a silent stop.
        package="$(dpkg -S "$(readlink -f "$path")" 2>/dev/null | awk -F: 'NR == 1 {print $1}' || true)"
        if [ -z "$package" ]; then
            echo "error: no package owns $path (needed by ${elf#"$APP_ROOT/"})" >&2
            exit 1
        fi
        DEP_PACKAGES["$package"]=1
    done < <(objdump -p "$elf" 2>/dev/null | awk '/NEEDED/ {print $2}')
done
DEPENDS="$(printf '%s\n' "${!DEP_PACKAGES[@]}" | sort | paste -sd, - | sed 's/,/, /g')"
# The update check (#117) uses HTTPS: Qt's TLS backend
# (plugins/tls/libqopensslbackend.so) loads libssl.so.3 when it starts (dlopen),
# and OpenSSL verifies the certificate of api.github.com with the certificates
# of ca-certificates. No NEEDED entry shows them, so name them here. libssl3t64
# on Ubuntu 24.04 and Debian 13; libssl3 is the name before the 64-bit time_t
# change.
[[ -f "$APP_ROOT/plugins/tls/libqopensslbackend.so" ]] || {
    echo "error: plugins/tls/libqopensslbackend.so is not in the install tree (the update check needs it)" >&2
    exit 1
}
DEPENDS="$DEPENDS, libssl3t64 | libssl3, ca-certificates"
echo "    Depends: $DEPENDS"

cat > "$ROOT/DEBIAN/control" <<EOF
Package: finance-dashboard
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: Nicolas Goyon <noreply@github.com>
Installed-Size: ${INSTALLED_SIZE_KB}
Depends: ${DEPENDS}
Homepage: https://github.com/nicolas-goyon/FinanceDashboard
Description: Local-first personal finance dashboard
 Import bank statements, categorize transactions and track net worth, cash
 flow, savings and investments over time.
 .
 All data stays on this machine in a local SQLite file — there is no bank
 connectivity and no account linking. The only network call is the update
 check: at each start, the app asks GitHub for the list of releases. The
 check can be turned off.
 .
 The Qt runtime this application needs is bundled inside the package, so it
 does not depend on the distribution's Qt version.
EOF

cat > "$ROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -f -t /usr/share/icons/hicolor || true
fi
EOF
chmod 755 "$ROOT/DEBIAN/postinst"
cp "$ROOT/DEBIAN/postinst" "$ROOT/DEBIAN/postrm"

mkdir -p "$OUT_DIR"
OUTPUT="$OUT_DIR/finance-dashboard_${VERSION}_${DEB_ARCH}.deb"

echo "==> Building $OUTPUT"
# --root-owner-group: no fakeroot needed, everything ends up root:root.
dpkg-deb --build --root-owner-group -Zxz "$ROOT" "$OUTPUT" >/dev/null

echo "==> Done: $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
dpkg-deb --info "$OUTPUT" | sed 's/^/    /'
