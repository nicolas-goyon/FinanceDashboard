#!/usr/bin/env bash
# ── FinanceDashboard launcher (Linux) ────────────────────────────────────────
#
# Shipped by docker/scripts/deploy.py as dist/<variant>/FinanceDashboard, next
# to the app/ folder. Checks the requirements listed in app/requirements.txt
# before starting app/FinanceDashboard: files present, x86_64 machine, and —
# via ldd — every system library (X11/xcb, GL, fontconfig, glibc version, …)
# the app and its Qt platform plugin need. If something is missing it lists
# every requirement as OK / FAIL (in a dialog when zenity/kdialog/xmessage is
# available, always on the terminal), otherwise it execs the app with the same
# arguments. In-app checks (after Qt is up) live in src/startup/ — see
# docs/startup.md.
set -u

HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
APP_DIR="${HERE}/app"
APP="${APP_DIR}/FinanceDashboard"
REQ="${APP_DIR}/requirements.txt"

lines=()
failed=0
ok()   { lines+=("[ OK ]  $*"); }
fail() { lines+=("[FAIL]  $*"); failed=1; }

# Debian/Ubuntu package that provides a few libraries Qt commonly needs,
# shown as a hint next to a missing library.
hint_for() {
    case "$1" in
        libxcb-cursor.so*)        echo "libxcb-cursor0" ;;
        libxkbcommon-x11.so*)     echo "libxkbcommon-x11-0" ;;
        libxkbcommon.so*)         echo "libxkbcommon0" ;;
        libxcb-icccm.so*)         echo "libxcb-icccm4" ;;
        libxcb-image.so*)         echo "libxcb-image0" ;;
        libxcb-keysyms.so*)       echo "libxcb-keysyms1" ;;
        libxcb-randr.so*)         echo "libxcb-randr0" ;;
        libxcb-render-util.so*)   echo "libxcb-render-util0" ;;
        libxcb-shape.so*)         echo "libxcb-shape0" ;;
        libxcb-xkb.so*)           echo "libxcb-xkb1" ;;
        libX11-xcb.so*)           echo "libx11-xcb1" ;;
        libGL.so*|libOpenGL.so*)  echo "libgl1" ;;
        libEGL.so*)               echo "libegl1" ;;
        libfontconfig.so*)        echo "libfontconfig1" ;;
        libfreetype.so*)          echo "libfreetype6" ;;
        libglib-2.0.so*|libgthread-2.0.so*) echo "libglib2.0-0" ;;
        libdbus-1.so*)            echo "libdbus-1-3" ;;
        *) echo "" ;;
    esac
}

# ── Machine ──────────────────────────────────────────────────────────────────
arch="$(uname -m)"
if [[ "${arch}" == "x86_64" ]]; then ok "Machine architecture: ${arch}"
else fail "Machine architecture: ${arch} (the app is built for x86_64)"; fi

if [[ -x "${APP}" ]]; then ok "Application: ${APP}"
else fail "Application: ${APP} is missing or not executable"; fi

# ── Files + libraries from requirements.txt ──────────────────────────────────
if [[ ! -f "${REQ}" ]]; then
    fail "Requirements list ${REQ} is missing (incomplete copy of the folder?)"
else
    total=0; missing=()
    lib_targets=()
    while IFS=' ' read -r key value; do
        value="${value%$'\r'}"
        case "${key}" in
            file)        total=$((total + 1)); [[ -e "${APP_DIR}/${value}" ]] || missing+=("${value}") ;;
            check-libs)  lib_targets+=("${value}") ;;
        esac
    done < <(grep -v '^[[:space:]]*#' "${REQ}")
    if [[ ${#missing[@]} -eq 0 ]]; then ok "Application files: all ${total} present"
    else fail "Application files: ${#missing[@]} of ${total} missing: ${missing[*]:0:15}"; fi

    if ! command -v ldd >/dev/null 2>&1; then
        fail "System libraries: 'ldd' not found, cannot check (install libc-bin)"
    else
        declare -A seen=()
        problems=()
        for t in "${lib_targets[@]}"; do
            [[ -e "${APP_DIR}/${t}" ]] || continue
            while IFS= read -r l; do
                # "libfoo.so.1 => not found"  or  "…: version `GLIBC_2.35' not found (required by …)"
                if [[ "${l}" == *"=> not found"* ]]; then
                    so="$(echo "${l}" | awk '{print $1}')"
                    [[ -n "${seen[${so}]:-}" ]] && continue; seen[${so}]=1
                    pkg="$(hint_for "${so}")"
                    problems+=("${so} not found${pkg:+ (Debian/Ubuntu package: ${pkg})}")
                elif [[ "${l}" == *"version"*"not found"* ]]; then
                    v="$(echo "${l}" | grep -o "version \`[^']*'" | head -1)"
                    [[ -n "${seen[${v}]:-}" ]] && continue; seen[${v}]=1
                    problems+=("${v#version } not found — this Linux is older than the build environment (Ubuntu 22.04: glibc 2.35, GCC 11)")
                fi
            done < <(LD_LIBRARY_PATH="${APP_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" ldd "${APP_DIR}/${t}" 2>&1)
        done
        if [[ ${#problems[@]} -eq 0 ]]; then ok "System libraries: all found (${#lib_targets[@]} binaries checked)"
        else
            fail "System libraries: ${#problems[@]} problem(s):"
            for p in "${problems[@]}"; do lines+=("          ${p}"); done
        fi
    fi
fi

# ── Display ──────────────────────────────────────────────────────────────────
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then ok "Display: ${WAYLAND_DISPLAY:+Wayland }${DISPLAY:+X11 ${DISPLAY}}"
else fail "Display: no DISPLAY / WAYLAND_DISPLAY set (no graphical session)"; fi

if [[ ${failed} -eq 0 ]]; then
    exec "${APP}" "$@"
fi

report="Finance Dashboard can't start on this computer.

Requirements:
$(printf '%s\n' "${lines[@]}")

See docs/startup.md."
echo "${report}" >&2
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
    if command -v zenity >/dev/null 2>&1; then zenity --error --no-markup --title="Finance Dashboard" --text="${report}" 2>/dev/null
    elif command -v kdialog >/dev/null 2>&1; then kdialog --title "Finance Dashboard" --error "${report}" 2>/dev/null
    elif command -v xmessage >/dev/null 2>&1; then xmessage -center "${report}" 2>/dev/null
    fi
fi
exit 1
