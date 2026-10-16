#!/usr/bin/env bash
# Build FinanceDashboard in Docker using the options in .env.
# See docs/docker-build.md.
#
#   ./tools/docker-build.sh                # check requirements, build the image if needed, then the app
#   ./tools/docker-build.sh --no-image     # skip the image step (faster when only code changed)
#   ./tools/docker-build.sh --skip-checks  # don't run the requirement check (overrides CHECK_REQUIREMENTS)
#   ./tools/docker-build.sh --force        # build even if the check fails (overrides FORCE_BUILD)
#
# Result: dist/<TARGET>-<LINK_MODE>-<build type>/
# Logs:   logs/docker-build/<yyyy-mm-dd_HH-MM-SS>_<TARGET>-<LINK_MODE>-<build type>/
#           summary.txt, env.txt, requirements.log, image.log, container.log,
#           configure.log, build.log, deploy.log (see tools/docker-build.ps1)
set -euo pipefail
cd "$(dirname "$0")/.." || exit 2
ROOT="$(pwd)"

NO_IMAGE=0; SKIP_CHECKS=0; FORCE_ARG=0
for a in "$@"; do
    case "$a" in
        --no-image)    NO_IMAGE=1 ;;
        --skip-checks) SKIP_CHECKS=1 ;;
        --force)       FORCE_ARG=1 ;;
        *) echo "unknown option: $a" >&2; exit 2 ;;
    esac
done

if [[ ! -f .env ]]; then
    cp .env.example .env
    echo "Created .env from .env.example - edit it to change target / link mode."
fi
opt() { # opt KEY DEFAULT
    local v
    v="$(grep -E "^[[:space:]]*$1[[:space:]]*=" .env | tail -1 | cut -d= -f2- | tr -d '\r' | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' -e 's/^["'\'']//' -e 's/["'\'']$//')" || true
    echo "${v:-$2}"
}
TARGET="$(opt TARGET windows)"; LINK="$(opt LINK_MODE shared)"; BUILD_TYPE="$(opt BUILD_TYPE Release)"
DO_CHECK=1; case "$(opt CHECK_REQUIREMENTS true)" in false|0|no|off) DO_CHECK=0 ;; esac
FORCE=0;    case "$(opt FORCE_BUILD false)" in true|1|yes|on) FORCE=1 ;; esac
(( SKIP_CHECKS )) && DO_CHECK=0
(( FORCE_ARG )) && FORCE=1

# ── Run log folder ───────────────────────────────────────────────────────────
VARIANT="${TARGET}-${LINK}-${BUILD_TYPE,,}"
RUN_NAME="$(date '+%Y-%m-%d_%H-%M-%S')_${VARIANT}"
RUN_DIR="${ROOT}/logs/docker-build/${RUN_NAME}"
mkdir -p "${RUN_DIR}"
cp .env "${RUN_DIR}/env.txt"
SUMMARY="${RUN_DIR}/summary.txt"
printf 'FinanceDashboard Docker build %s\nTARGET=%s LINK_MODE=%s BUILD_TYPE=%s CHECK_REQUIREMENTS=%s FORCE_BUILD=%s\n\n' \
    "${RUN_NAME}" "${TARGET}" "${LINK}" "${BUILD_TYPE}" "${DO_CHECK}" "${FORCE}" > "${SUMMARY}"
echo "Logs: logs/docker-build/${RUN_NAME}"

step() { # step NAME LOGFILE CMD... — run, tee to the log, record result + duration
    local name="$1" logf="$2"; shift 2
    local start=${SECONDS} code=0
    if [[ -n "${logf}" ]]; then "$@" 2>&1 | tee -a "${logf}" || code=$?
    else "$@" || code=$?; fi
    local d=$((SECONDS - start))
    printf '%-14s %-8s %02d:%02d:%02d\n' "${name}" "$([[ ${code} -eq 0 ]] && echo OK || echo "FAIL(${code})")" \
        $((d / 3600)) $((d % 3600 / 60)) $((d % 60)) >> "${SUMMARY}"
    return "${code}"
}

# ── 1. Requirement check ─────────────────────────────────────────────────────
if (( DO_CHECK )); then
    if ! step requirements "" ./tools/check-requirements.sh --log-file "${RUN_DIR}/requirements.log"; then
        if (( FORCE )); then
            echo "Requirement check failed - continuing anyway (FORCE_BUILD / --force)."
            echo "               (continued: FORCE_BUILD)" >> "${SUMMARY}"
        else
            echo "Stopped: requirement check failed. Fix the FAIL lines, or set FORCE_BUILD=true / use --force." | tee -a "${SUMMARY}" >&2
            exit 1
        fi
    fi
else
    echo "requirements   SKIPPED (CHECK_REQUIREMENTS=false / --skip-checks)" >> "${SUMMARY}"
fi

# ── 2. Toolchain image ───────────────────────────────────────────────────────
if (( ! NO_IMAGE )); then
    step image "${RUN_DIR}/image.log" docker compose build builder \
        || { echo "docker compose build failed - see logs/docker-build/${RUN_NAME}/image.log" >&2; exit 1; }
fi

# ── 3. App build (configure / build / deploy inside the container) ──────────
export RUN_LOG_DIR="docker-build/${RUN_NAME}"
step app "${RUN_DIR}/container.log" docker compose run --rm -T builder \
    || { echo "build failed - see logs/docker-build/${RUN_NAME}/" >&2; exit 1; }
printf '\nResult: dist/%s/\n' "${VARIANT}" >> "${SUMMARY}"
echo "Done: dist/${VARIANT}/  (logs: logs/docker-build/${RUN_NAME})"
