#!/usr/bin/env bash
# ── test-deb.sh ───────────────────────────────────────────────────────────────
# Install the .deb on a clean Debian/Ubuntu system and check that the app can
# start. Run it as root in a fresh container, so that only the package and its
# Depends are installed:
#
#   docker run --rm -v "$PWD:/src:ro" ubuntu:24.04 \
#       bash /src/packaging/linux/test-deb.sh /src/dist/finance-dashboard_<version>_amd64.deb
#
#   1. apt installs the package and the packages of its Depends line.
#   2. Each ELF file that the app loads at start-up must find its libraries.
#      A library that is not bundled and that no Depends package provides
#      shows as "not found" here: add its package to make-deb.sh.
#   3. With Xvfb added, the app must still run after 20 s on the real xcb
#      platform plugin. Then the package is removed.
set -euo pipefail

DEB="${1:?usage: test-deb.sh <package.deb>}"
APP=/opt/FinanceDashboard
export DEBIAN_FRONTEND=noninteractive

echo "==> Installing $DEB and its dependencies"
apt-get update -qq
apt-get install -y -qq --no-install-recommends "$DEB" >/dev/null
test -x "$APP/bin/FinanceDashboard"
test -L /usr/bin/finance-dashboard

echo "==> Checking that each library is found"
missing=0
while IFS= read -r -d '' elf; do
    while read -r lib; do
        [ -e "$APP/lib/$lib" ] && continue   # bundled: found at run time
        echo "  $elf: $lib not found"
        missing=1
    done < <(ldd "$elf" 2>/dev/null | awk '/not found/ {print $1}')
done < <(find "$APP/bin" "$APP/lib" "$APP/plugins/platforms" \
              "$APP/plugins/xcbglintegrations" "$APP/plugins/sqldrivers/libqsqlite.so" \
              "$APP/qml" -type f \( -name '*.so*' -o -name FinanceDashboard \) -print0)
if [ "$missing" -ne 0 ]; then
    echo "error: add the packages that provide these libraries to Depends in packaging/linux/make-deb.sh" >&2
    exit 1
fi

echo "==> Starting the app under Xvfb (real xcb platform plugin)"
apt-get install -y -qq --no-install-recommends xvfb xauth >/dev/null
set +e
xvfb-run -a --server-args="-screen 0 1280x800x24" timeout --signal=TERM 20 finance-dashboard
rc=$?
set -e
# 124/143 = still running when the timeout fired, which is the pass.
if [ "$rc" -ne 124 ] && [ "$rc" -ne 143 ]; then
    echo "error: the app exited early with code $rc" >&2
    exit 1
fi

apt-get remove -y -qq finance-dashboard >/dev/null
test ! -e "$APP/bin/FinanceDashboard"
echo "==> OK: the .deb installs on a clean system, and the app stays up for 20 s"
