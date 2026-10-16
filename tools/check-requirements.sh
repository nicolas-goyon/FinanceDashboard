#!/usr/bin/env bash
# Check, BEFORE building, that everything the Docker build needs is in place
# and whether THIS computer will be able to run the result - based on the
# options in .env (TARGET, LINK_MODE, QT_VERSION, LLVM_MINGW_VERSION, ...).
# Linux / macOS counterpart of tools/check-requirements.ps1 — same checks.
# See docs/docker-build.md, section "Requirement check".
#
#   ./tools/check-requirements.sh                    # report only
#   ./tools/check-requirements.sh --log-file x.log   # also write the report to a file
#   ./tools/check-requirements.sh --offline          # skip the download/version checks
#
# Exit code: 0 = no failure (warnings allowed), 1 = at least one FAIL.
# tools/docker-build.sh runs this automatically (CHECK_REQUIREMENTS / FORCE_BUILD in .env).
set -uo pipefail
cd "$(dirname "$0")/.." || exit 2
ROOT="$(pwd)"

LOG_FILE=""
OFFLINE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --log-file) LOG_FILE="$2"; shift 2 ;;
        --offline)  OFFLINE=1; shift ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

# ── Options (.env, falling back to .env.example, then defaults) ──────────────
ENV_FILE="${ROOT}/.env"
[[ -f "${ENV_FILE}" ]] || ENV_FILE="${ROOT}/.env.example"
getenv() { # getenv KEY DEFAULT
    local v=""
    [[ -f "${ENV_FILE}" ]] && v="$(grep -E "^[[:space:]]*$1[[:space:]]*=" "${ENV_FILE}" | tail -1 | cut -d= -f2- | tr -d '\r' | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' -e 's/^["'\'']//' -e 's/["'\'']$//')"
    echo "${v:-$2}"
}
TARGET="$(getenv TARGET windows)"
LINK="$(getenv LINK_MODE shared)"
BUILD_TYPE="$(getenv BUILD_TYPE Release)"
QTV="$(getenv QT_VERSION 6.11.1)"
LLVMV="$(getenv LLVM_MINGW_VERSION 20231128)"

# ── Reporting ────────────────────────────────────────────────────────────────
LOG_LINES=()
FAILS=0; WARNS=0
if [[ -t 1 ]]; then C_OK=$'\e[32m'; C_WARN=$'\e[33m'; C_FAIL=$'\e[31m'; C_SKIP=$'\e[90m'; C_H=$'\e[36m'; C_0=$'\e[0m'
else C_OK=""; C_WARN=""; C_FAIL=""; C_SKIP=""; C_H=""; C_0=""; fi
out() { echo "${2:-}$1${2:+${C_0}}"; LOG_LINES+=("$1"); }
section() { out ""; out "== $1 ==" "${C_H}"; }
report() { # report STATUS NAME DETAIL
    local color="${C_OK}"
    case "$1" in WARN) color="${C_WARN}"; WARNS=$((WARNS+1)) ;; FAIL) color="${C_FAIL}"; FAILS=$((FAILS+1)) ;; SKIP) color="${C_SKIP}" ;; esac
    out "$(printf '  [%-4s] %s: %s' "$1" "$2" "$3")" "${color}"
}
get_text() { curl -fsL --max-time 30 "$1"; }
url_ok()  { curl -fsIL --max-time 30 -o /dev/null "$1"; }
ver_ge()  { [[ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -1)" == "$2" ]]; } # ver_ge A B: A >= B

out "FinanceDashboard requirement check - $(date '+%Y-%m-%d %H:%M:%S')"
out "Options from $(basename "${ENV_FILE}"): TARGET=${TARGET} LINK_MODE=${LINK} BUILD_TYPE=${BUILD_TYPE} QT_VERSION=${QTV} LLVM_MINGW_VERSION=${LLVMV}"

# ── 1. Options ───────────────────────────────────────────────────────────────
section "Options (.env)"
case "${TARGET}" in windows|linux) report OK TARGET "${TARGET}" ;; *) report FAIL TARGET "'${TARGET}' (expected windows or linux)" ;; esac
case "${LINK}" in shared|static) report OK LINK_MODE "${LINK}" ;; *) report FAIL LINK_MODE "'${LINK}' (expected shared or static)" ;; esac
case "${BUILD_TYPE}" in Release|Debug|RelWithDebInfo|MinSizeRel) report OK BUILD_TYPE "${BUILD_TYPE}" ;;
    *) report FAIL BUILD_TYPE "'${BUILD_TYPE}' (expected Release, Debug, RelWithDebInfo or MinSizeRel)" ;; esac
QT_OK=0
if [[ "${QTV}" =~ ^6\.([0-9]+)\.[0-9]+$ ]]; then
    if (( BASH_REMATCH[1] >= 8 )); then report OK QT_VERSION "${QTV}"; QT_OK=1
    else report FAIL QT_VERSION "${QTV} - the project needs Qt 6.8 or later (qt_standard_project_setup(REQUIRES 6.8))"; fi
else report FAIL QT_VERSION "'${QTV}' (expected 6.x.y)"; fi

# ── 2. Build machine (Docker) ────────────────────────────────────────────────
section "Build machine (Docker)"
if ! command -v docker >/dev/null 2>&1; then
    report FAIL "Docker CLI" "not found - install Docker: https://docs.docker.com/get-docker/"
else
    report OK "Docker CLI" "$(docker --version 2>/dev/null)"
    if ! os_type="$(docker info --format '{{.OSType}}' 2>/dev/null)"; then
        report FAIL "Docker engine" "not running (or no permission - add yourself to the 'docker' group)"
    else
        [[ "${os_type}" == "linux" ]] && report OK "Container mode" "Linux containers" \
            || report FAIL "Container mode" "${os_type} containers - Linux containers are required"
        mem="$(docker info --format '{{.MemTotal}}' 2>/dev/null)"; cpus="$(docker info --format '{{.NCPU}}' 2>/dev/null)"
        mem_gb=$(( ${mem:-0} / 1073741824 )); need_gb=4; [[ "${LINK}" == "static" ]] && need_gb=8
        if (( mem_gb >= need_gb )); then report OK "Docker memory" "${mem_gb} GB, ${cpus} CPUs"
        else report WARN "Docker memory" "${mem_gb} GB (recommended >= ${need_gb} GB for ${LINK}; or lower QT_BUILD_JOBS/BUILD_JOBS)"; fi
    fi
    if cv="$(docker compose version --short 2>/dev/null)"; then report OK "Docker Compose" "v${cv}"
    else report FAIL "Docker Compose" "'docker compose' (v2 plugin) not available"; fi
    root_dir="$(docker info --format '{{.DockerRootDir}}' 2>/dev/null || true)"
fi
need_disk=8; [[ "${LINK}" == "static" ]] && need_disk=30
for d in "${ROOT}" ${root_dir:+"${root_dir}"}; do
    [[ -d "${d}" ]] || continue
    free_gb=$(( $(df -Pk "${d}" | awk 'NR==2 {print $4}') / 1048576 ))
    if (( free_gb >= need_disk )); then report OK "Free disk (${d})" "${free_gb} GB"
    else report WARN "Free disk (${d})" "${free_gb} GB (recommended >= ${need_disk} GB for a ${LINK} build)"; fi
done

# ── 3. Downloads & versions ──────────────────────────────────────────────────
section "Downloads & versions"
if (( OFFLINE )); then report SKIP Downloads "--offline given"
elif (( ! QT_OK )); then report SKIP Downloads "QT_VERSION invalid"
else
    nodot="${QTV//./}"
    repo="https://download.qt.io/online/qtsdkrepository"
    qt_llvm=""
    if ! [[ "${TARGET}" == "linux" && "${LINK}" == "static" ]]; then
        if xml="$(get_text "${repo}/linux_x64/desktop/qt6_${nodot}/qt6_${nodot}/Updates.xml")"; then
            grep -q "qt\.qt6\.${nodot}\.addons\.qtgraphs\.linux_gcc_64" <<<"${xml}" \
                && report OK "Qt (Linux, prebuilt)" "${QTV} linux_gcc_64 with Qt Graphs" \
                || report FAIL "Qt (Linux, prebuilt)" "${QTV} linux_gcc_64 exists but has no Qt Graphs package"
        else report FAIL "Qt (Linux, prebuilt)" "${QTV} linux_gcc_64 not found on download.qt.io"; fi
    fi
    if [[ "${LINK}" == "shared" && "${TARGET}" == "windows" ]]; then
        xml=""
        for sub in "qt6_${nodot}_llvm_mingw" "qt6_${nodot}"; do   # Qt >= 6.11 splits the Windows repo per compiler
            xml="$(get_text "${repo}/windows_x86/desktop/qt6_${nodot}/${sub}/Updates.xml")" && break
        done
        if grep -q "qt\.qt6\.${nodot}\.addons\.qtgraphs\.win64_llvm_mingw" <<<"${xml}"; then
            qt_llvm="$(grep -oE 'llvm-mingw[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+' <<<"${xml}" | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
            report OK "Qt (Windows, prebuilt)" "${QTV} win64_llvm_mingw with Qt Graphs${qt_llvm:+, built with LLVM ${qt_llvm}}"
        else report FAIL "Qt (Windows, prebuilt)" "${QTV} win64_llvm_mingw (with Qt Graphs) not found on download.qt.io"; fi
    fi
    if [[ "${LINK}" == "static" ]]; then
        mm="${QTV%.*}"; found=""
        for base in "https://download.qt.io/official_releases/qt/${mm}/${QTV}/submodules" "https://download.qt.io/archive/qt/${mm}/${QTV}/submodules"; do
            url_ok "${base}/qtbase-everywhere-src-${QTV}.tar.xz" && { found="${base}"; break; }
        done
        if [[ -z "${found}" ]]; then report FAIL "Qt sources" "${QTV} source tarballs not found (official_releases/ or archive/)"
        else
            missing=()
            for m in qtshadertools qtdeclarative qtgraphs; do url_ok "${found}/${m}-everywhere-src-${QTV}.tar.xz" || missing+=("${m}"); done
            (( ${#missing[@]} == 0 )) && report OK "Qt sources" "qtbase, qtshadertools, qtdeclarative, qtgraphs ${QTV}" \
                || report FAIL "Qt sources" "missing: ${missing[*]}"
        fi
    fi
    if [[ "${TARGET}" == "windows" ]]; then
        if rel="$(get_text "https://api.github.com/repos/mstorsjo/llvm-mingw/releases/tags/${LLVMV}")"; then
            rel_name="$(grep -m1 '"name"' <<<"${rel}" | sed -E 's/.*"name": *"([^"]*)".*/\1/')"
            rel_llvm="$(grep -oE 'LLVM [0-9]+\.[0-9]+\.[0-9]+' <<<"${rel_name}" | grep -oE '[0-9.]+$')"
            if ! grep -q "\"llvm-mingw-${LLVMV}-ucrt-ubuntu-20.04-x86_64.tar.xz\"" <<<"${rel}"; then
                report FAIL llvm-mingw "release ${LLVMV} has no ucrt-ubuntu-20.04-x86_64 package"
            else
                report OK llvm-mingw "release ${LLVMV} (${rel_name})"
                if [[ "${LINK}" == "static" ]]; then report SKIP "LLVM version match" "not needed for static: Qt is compiled from source with this same toolchain"
                elif [[ -z "${qt_llvm}" || -z "${rel_llvm}" ]]; then report WARN "LLVM version match" "couldn't read both versions (Qt: ${qt_llvm}, llvm-mingw: ${rel_llvm}) - check by hand"
                elif [[ "${qt_llvm}" == "${rel_llvm}" ]]; then report OK "LLVM version match" "Qt's binaries and llvm-mingw ${LLVMV} are both LLVM ${rel_llvm}"
                else report FAIL "LLVM version match" "Qt ${QTV} was built with LLVM ${qt_llvm} but llvm-mingw ${LLVMV} is LLVM ${rel_llvm} - set LLVM_MINGW_VERSION to the llvm-mingw release with LLVM ${qt_llvm}"; fi
            fi
        else report FAIL llvm-mingw "release '${LLVMV}' not found on GitHub"; fi
    fi
    url_ok "https://pypi.org/simple/aqtinstall/" && report OK PyPI "reachable (aqtinstall dependencies, CMake)" \
        || report WARN PyPI "pypi.org not reachable from this computer - the image build needs it"
fi

# ── 4. Can THIS computer run the result? ─────────────────────────────────────
section "Running the app on this computer (${TARGET}, ${LINK})"
host_os="$(uname -s)"
if [[ "${TARGET}" == "windows" ]]; then
    report SKIP "Run check" "TARGET=windows can't run on ${host_os} - run ./tools/check-requirements.ps1 on the Windows PC, or just start the built app there (its launcher checks everything)"
elif [[ "${host_os}" != "Linux" ]]; then
    report SKIP "Run check" "TARGET=linux can't run on ${host_os}"
else
    arch="$(uname -m)"
    [[ "${arch}" == "x86_64" ]] && report OK "CPU architecture" "x86_64" || report FAIL "CPU architecture" "${arch} - the app is x86_64 only"
    # The build image is Ubuntu 22.04 (glibc 2.35, GCC 11): binaries need at least that glibc.
    glibc="$(ldd --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+$')"
    if [[ -z "${glibc}" ]]; then report WARN glibc "couldn't detect the glibc version (musl-based distro?) - the app needs glibc 2.35+"
    elif ver_ge "${glibc}" "2.35"; then report OK glibc "${glibc} (needs 2.35+)"
    else report FAIL glibc "${glibc} - the app is built on Ubuntu 22.04 and needs glibc 2.35+ (Ubuntu 22.04+, Debian 12+, Fedora 36+)"; fi
    if [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then report OK Display "${WAYLAND_DISPLAY:+Wayland }${DISPLAY:+X11 ${DISPLAY}}"
    else report WARN Display "no DISPLAY / WAYLAND_DISPLAY in this shell (fine if you'll start the app from a desktop session)"; fi
    # System libraries Qt's xcb platform plugin + Qt Gui need (not shipped with the app).
    # Static builds don't use glib (Qt built without it); shared ones (Qt's own binaries) do.
    libs=(
        "libxcb-cursor.so.0:libxcb-cursor0" "libxkbcommon.so.0:libxkbcommon0" "libxkbcommon-x11.so.0:libxkbcommon-x11-0"
        "libxcb-icccm.so.4:libxcb-icccm4" "libxcb-image.so.0:libxcb-image0" "libxcb-keysyms.so.1:libxcb-keysyms1"
        "libxcb-randr.so.0:libxcb-randr0" "libxcb-render-util.so.0:libxcb-render-util0" "libxcb-shape.so.0:libxcb-shape0"
        "libxcb-xkb.so.1:libxcb-xkb1" "libX11-xcb.so.1:libx11-xcb1" "libGL.so.1:libgl1" "libEGL.so.1:libegl1"
        "libfontconfig.so.1:libfontconfig1" "libfreetype.so.6:libfreetype6"
    )
    [[ "${LINK}" == "shared" ]] && libs+=("libglib-2.0.so.0:libglib2.0-0" "libdbus-1.so.3:libdbus-1-3")
    cache="$( (ldconfig -p 2>/dev/null || /sbin/ldconfig -p 2>/dev/null) )"
    missing=(); pkgs=()
    for entry in "${libs[@]}"; do
        so="${entry%%:*}"; pkg="${entry##*:}"
        grep -q "${so} " <<<"${cache}" || { missing+=("${so}"); pkgs+=("${pkg}"); }
    done
    if (( ${#missing[@]} == 0 )); then report OK "System libraries" "all ${#libs[@]} present"
    else report FAIL "System libraries" "missing ${missing[*]} - Debian/Ubuntu: sudo apt install ${pkgs[*]}"; fi
    if [[ "${LINK}" == "shared" ]]; then report OK "Bundled libraries" "Qt + ICU ship in app/lib, nothing to install"
    else report OK "Bundled libraries" "Qt is linked into the executable, nothing to install"; fi
fi

# ── Summary ──────────────────────────────────────────────────────────────────
out ""
if (( FAILS == 0 )); then out "RESULT: OK (${WARNS} warning(s))" "${C_OK}"
else out "RESULT: ${FAILS} requirement(s) not met, ${WARNS} warning(s)" "${C_FAIL}"; fi
if [[ -n "${LOG_FILE}" ]]; then mkdir -p "$(dirname "${LOG_FILE}")"; printf '%s\n' "${LOG_LINES[@]}" > "${LOG_FILE}"; fi
(( FAILS == 0 ))
