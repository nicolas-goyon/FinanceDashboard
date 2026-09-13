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
# The package therefore Depends on almost nothing: the Qt libraries it needs
# travel inside it, exactly like the AppImage and the Windows installer. That
# is the point — the user asked for plug and play, not for a package that
# pulls half of Qt from the distribution's repositories and then disagrees with
# it about versions. It is *not* how a package accepted into Debian proper
# would be built; if this ever goes to a real repository, that version links
# against the distro's Qt instead.
#
# Why not CPack's DEB generator: it installs under a single prefix, so the
# /opt payload plus the four /usr desktop-integration files would need two CPack
# runs and a component split for what is twenty readable lines of shell here.
#
# Needs: dpkg-deb (Debian/Ubuntu), fakeroot optional.
set -euo pipefail

usage() { sed -n '2,30p' "$0" >&2; exit 2; }

INSTALL_TREE="${1:-}"
VERSION="${2:-}"
OUT_DIR="${3:-dist}"
[[ -n "$INSTALL_TREE" && -n "$VERSION" ]] || usage
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

cat > "$ROOT/DEBIAN/control" <<EOF
Package: finance-dashboard
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: Nicolas Goyon <noreply@github.com>
Installed-Size: ${INSTALLED_SIZE_KB}
Depends: libc6, libstdc++6, libgl1, libfontconfig1, libfreetype6, libx11-6, libxkbcommon0
Homepage: https://github.com/nicolas-goyon/FinanceDashboard
Description: Local-first personal finance dashboard
 Import bank statements, categorize transactions and track net worth, cash
 flow, savings and investments over time.
 .
 All data stays on this machine in a local SQLite file — there is no bank
 connectivity, no account linking and no external data fetching.
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
