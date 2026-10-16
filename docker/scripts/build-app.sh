#!/usr/bin/env bash
# Container entrypoint: configure + build + package FinanceDashboard.
# Driven by environment variables that compose.yaml forwards from .env:
#
#   TARGET      windows | linux
#   LINK_MODE   shared  | static
#   BUILD_TYPE  Release | Debug | RelWithDebInfo | MinSizeRel   (default Release)
#   BUILD_JOBS  parallel compile jobs (empty = all cores)
#
# Mounts (see compose.yaml):
#   /project     the repo, read-only
#   /work/build  persistent build trees (host: ./build-docker)
#   /work/dist   packaged output        (host: ./dist)
#   /work/logs   run logs               (host: ./logs) — RUN_LOG_DIR picks the
#                per-run folder; without it (plain `docker compose run`) a
#                timestamped one is created here.
set -euo pipefail

TARGET="${TARGET:-windows}"
LINK_MODE="${LINK_MODE:-shared}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${BUILD_JOBS:-}"

if [[ "${TARGET}" != "${IMAGE_TARGET}" || "${LINK_MODE}" != "${IMAGE_LINK_MODE}" ]]; then
    echo "error: this image was built for ${IMAGE_TARGET}/${IMAGE_LINK_MODE}, but .env asks for" \
         "${TARGET}/${LINK_MODE}. Rebuild the image (docker compose build builder)." >&2
    exit 1
fi

VARIANT="${TARGET}-${LINK_MODE}-${BUILD_TYPE,,}"
SRC=/work/build/${VARIANT}/src
BUILD=/work/build/${VARIANT}/build
OUT=/work/dist/${VARIANT}

LOG_DIR="/work/logs/${RUN_LOG_DIR:-docker-build/$(date '+%Y-%m-%d_%H-%M-%S')_${VARIANT}}"
mkdir -p "${LOG_DIR}"
# run_logged NAME CMD... — run a step, show its output and keep it in NAME.log
run_logged() {
    local name="$1"; shift
    echo "==> ${name}  (log: ${LOG_DIR#/work/}/${name}.log)"
    "$@" 2>&1 | tee "${LOG_DIR}/${name}.log"
}

echo "==> FinanceDashboard: ${VARIANT} (Qt ${IMAGE_QT_VERSION})"

# ── 1. Snapshot the sources ──────────────────────────────────────────────────
# Build from a private copy, not /project directly:
#  - /project is mounted read-only, and nothing lands in the user's tree
#    except ./build-docker and ./dist;
#  - Qt's QML import scan (qmlimportscanner) walks the *whole* source dir
#    recursively, so build trees / deployed Qt QML folders lying around in the
#    repo (dist/, cmake-build-*/) would otherwise get scanned too — which, in
#    a static build, means linking every QML plugin found there.
# rsync keeps file mtimes, so incremental rebuilds stay incremental.
mkdir -p "${SRC}" "${BUILD}"
rsync -a --delete \
    --exclude '/.git/' --exclude '/.idea/' --exclude '/.vs/' \
    --exclude '/build-docker/' --exclude '/dist/' --exclude '/build/' \
    --exclude '/cmake-build-*/' --exclude '/out/' --exclude '/logs/' \
    --exclude '/docker/' --exclude '__pycache__/' \
    /project/ "${SRC}/"

# ── 2. Configure ─────────────────────────────────────────────────────────────
CMAKE_ARGS=(
    -S "${SRC}" -B "${BUILD}" -G Ninja
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_PREFIX_PATH=/opt/qt-target
)
if [[ "${TARGET}" == "windows" ]]; then
    # QT_HOST_PATH: the Linux Qt whose moc/qmlcachegen/qmltyperegistrar/… run
    # during the build (Qt's documented cross-compiling mechanism).
    CMAKE_ARGS+=(
        -DCMAKE_TOOLCHAIN_FILE=/opt/fdbuild/toolchain-llvm-mingw.cmake
        -DQT_HOST_PATH=/opt/qt-host
        -DQT_HOST_PATH_CMAKE_DIR=/opt/qt-host/lib/cmake
    )
    if [[ "${LINK_MODE}" == "static" ]]; then
        # Also link libc++/libunwind/winpthread statically → no DLL at all.
        CMAKE_ARGS+=(-DCMAKE_EXE_LINKER_FLAGS=-static)
    fi
fi
run_logged configure cmake "${CMAKE_ARGS[@]}"

# ── 3. Build ─────────────────────────────────────────────────────────────────
run_logged build cmake --build "${BUILD}" --parallel ${JOBS}

# ── 4. Package into dist/<variant>/ ──────────────────────────────────────────
run_logged deploy python3 /opt/fdbuild/deploy.py \
    --target "${TARGET}" --link-mode "${LINK_MODE}" \
    --build-dir "${BUILD}" --src-dir "${SRC}" --out-dir "${OUT}" \
    --qt-target /opt/qt-target --qt-host /opt/qt-host

echo "==> Done: dist/${VARIANT}/"
