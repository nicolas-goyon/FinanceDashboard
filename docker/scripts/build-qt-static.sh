#!/usr/bin/env bash
# Build a *static* Qt from the official source tarballs, inside the Docker
# image (called from docker/Dockerfile, toolchain-*-static stages).
#
#   build-qt-static.sh <linux|windows> <install-prefix>
#
# Env: QT_VERSION (required), QT_BUILD_JOBS (optional, empty = all cores).
#
# Only the Qt repositories this app needs are built, in dependency order:
#   qtbase → qtshadertools → qtdeclarative → qtgraphs
# (qtquick3d is an *optional* dependency of qtgraphs — see qtgraphs'
# dependencies.yaml — and is skipped: this app only uses 2D Qt Graphs, and
# quick3d alone would roughly double the build time.)
#
# Configure options are the documented ones from
# https://doc.qt.io/qt-6/configure-options.html / qtbase's config_help.txt.
# Cross-compiling (windows) follows Qt's documented pattern: a host Qt of the
# exact same version via -qt-host-path, plus a CMake toolchain file after "--".
set -euo pipefail

TARGET="${1:?usage: build-qt-static.sh <linux|windows> <prefix>}"
PREFIX="${2:?usage: build-qt-static.sh <linux|windows> <prefix>}"
: "${QT_VERSION:?QT_VERSION must be set}"
JOBS="${QT_BUILD_JOBS:-}"

MODULES=(qtbase qtshadertools qtdeclarative qtgraphs)
QT_MAJOR_MINOR="${QT_VERSION%.*}"
# Current releases live under official_releases/, older ones are moved to
# archive/ — try both.
BASE_URL=""
for candidate in \
    "https://download.qt.io/official_releases/qt/${QT_MAJOR_MINOR}/${QT_VERSION}/submodules" \
    "https://download.qt.io/archive/qt/${QT_MAJOR_MINOR}/${QT_VERSION}/submodules"; do
    if curl -fsIL "${candidate}/qtbase-everywhere-src-${QT_VERSION}.tar.xz" >/dev/null; then
        BASE_URL="${candidate}"
        break
    fi
done
if [[ -z "${BASE_URL}" ]]; then
    echo "error: Qt ${QT_VERSION} source tarballs not found on download.qt.io" >&2
    exit 1
fi
WORK=/tmp/qt-static-build

mkdir -p "${WORK}/src"
cd "${WORK}/src"
for m in "${MODULES[@]}"; do
    echo "==> Downloading ${m} ${QT_VERSION}"
    curl -fsSL "${BASE_URL}/${m}-everywhere-src-${QT_VERSION}.tar.xz" | tar -xJ
    mv "${m}-everywhere-src-${QT_VERSION}" "${m}"
done

# Bundle Qt's own copies of third-party libs so the result depends on as few
# system libraries as possible.
COMMON_OPTS=(
    -static -release
    -prefix "${PREFIX}"
    -nomake examples -nomake tests
    -qt-zlib -qt-libpng -qt-libjpeg -qt-pcre -qt-harfbuzz -qt-sqlite
)

case "${TARGET}" in
    windows)
        # -static-runtime: "With -static, use static runtime (Windows only)".
        # QT_QMAKE_TARGET_MKSPEC: Qt 6 has no -xplatform; the CMake-based
        # configure takes the target mkspec this way (as in Qt's own
        # cross-compiling guide), win32-clang-g++ = clang targeting MinGW.
        TARGET_OPTS=(
            -static-runtime
            -qt-host-path /opt/qt-host
            --
            -DCMAKE_TOOLCHAIN_FILE=/opt/fdbuild/toolchain-llvm-mingw.cmake
            -DQT_QMAKE_TARGET_MKSPEC=win32-clang-g++
        )
        ;;
    linux)
        # Native build. FreeType/Fontconfig stay system libs: Qt's fontconfig
        # feature requires system FreeType (qtbase src/gui/configure.cmake).
        TARGET_OPTS=()
        ;;
    *)
        echo "Unknown target '${TARGET}' (expected linux or windows)" >&2
        exit 1
        ;;
esac

echo "==> Configuring qtbase (${TARGET}, static)"
mkdir -p "${WORK}/build/qtbase"
cd "${WORK}/build/qtbase"
"${WORK}/src/qtbase/configure" "${COMMON_OPTS[@]}" "${TARGET_OPTS[@]}"
cmake --build . --parallel ${JOBS}
cmake --install .

# Remaining modules: qt-configure-module from the freshly installed qtbase
# (it already knows the prefix, the static setting, and — for windows — the
# toolchain and host Qt path).
for m in "${MODULES[@]:1}"; do
    echo "==> Building ${m}"
    mkdir -p "${WORK}/build/${m}"
    cd "${WORK}/build/${m}"
    "${PREFIX}/bin/qt-configure-module" "${WORK}/src/${m}"
    cmake --build . --parallel ${JOBS}
    cmake --install .
done

# Keep the image small: sources + build trees are several GB.
cd /
rm -rf "${WORK}"
echo "==> Static Qt ${QT_VERSION} (${TARGET}) installed to ${PREFIX}"
