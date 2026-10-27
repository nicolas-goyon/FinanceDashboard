# syntax=docker/dockerfile:1
#
# FinanceDashboard — containerized toolchain (Qt + compiler), one image per
# TARGET × LINK_MODE combination. The image holds the *toolchain only*; the
# project sources are mounted at run time by compose.yaml and built by
# docker/scripts/build-app.sh, so editing code never rebuilds this image.
#
# See docs/docker-build.md for the full design and the options in .env.
#
# Stage graph (BuildKit only builds the stages the selected final stage needs):
#
#   base ──► qt-host ──► toolchain-linux-shared            (aqt prebuilt Qt, Linux)
#     │         └──────► mingw ──► toolchain-windows-shared (aqt prebuilt Qt, win64_llvm_mingw)
#     │                    └─────► toolchain-windows-static (Qt built from source, cross)
#     └────────────────────────────► toolchain-linux-static (Qt built from source, native)
#
# Conventions inside every final image:
#   /opt/qt-target  Qt prefix the app is compiled against
#   /opt/qt-host    Linux Qt providing host tools (moc, qmlcachegen, qmlimportscanner…)
#                   — absent in linux-static, where /opt/qt-target has its own tools
#   /opt/llvm-mingw llvm-mingw cross toolchain (windows targets only)

ARG TARGET=windows
ARG LINK_MODE=shared
ARG QT_VERSION=6.11.1
ARG LLVM_MINGW_VERSION=20231128
# aqtinstall is installed from a pinned git commit, not PyPI: the latest
# release (3.3.0) can't find Qt >= 6.11 Windows packages, because Qt split the
# Windows repository into one folder per compiler
# (online/qtsdkrepository/windows_x86/desktop/qt6_6111/qt6_6111_llvm_mingw/).
# Fixed on aqtinstall's master branch (EXTENSIONS_WIN_X64_QT6_11) — switch back
# to a PyPI version once a release (> 3.3.0) includes it.
ARG AQT_REF=076e1659807d0b362a3ed684d54c2e9c775eb9c7
# CMake from PyPI: Ubuntu 22.04 ships 3.22.1, exactly Qt 6.11's minimum for
# building Qt itself — a newer one avoids sitting on that edge.
# Both are installed into a venv (/opt/pytools, symlinked into /usr/local/bin),
# NOT with the system pip: Ubuntu 22.04's pip 22.0.2 builds packages from
# source against the system setuptools 59 instead of the setuptools >= 80
# aqtinstall requires ("AttributeError: ignore_egg_info_in_manifest"). The
# same pip inside a venv isolates the build correctly.
ARG CMAKE_PIP_VERSION=3.31.6

# ── base: compilers, CMake/Ninja, Qt-for-X11 build deps, aqtinstall ─────────
# Ubuntu 22.04 (glibc 2.35, GCC 11), not 24.04: Linux binaries only run on
# systems with a glibc at least as new as the one they were built against, so
# an older base = the linux builds run on more distributions (Ubuntu 22.04+,
# Debian 12+, …). Qt's own Linux binaries need glibc 2.34
# (https://doc.qt.io/qt-6/supported-platforms.html).
FROM ubuntu:22.04 AS base
ARG AQT_REF
ARG CMAKE_PIP_VERSION
ARG DEBIAN_FRONTEND=noninteractive
# X11/xcb list = Qt's documented Linux build requirements
# (https://doc.qt.io/qt-6/linux-requirements.html), minus libgtk-3-dev.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils git file rsync patchelf \
        python3 python3-venv perl pkg-config \
        build-essential ninja-build \
        libgl-dev libegl-dev libglib2.0-0 libdbus-1-3 \
        libfontconfig1-dev libfreetype-dev \
        libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev \
        libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev libxcb-icccm4-dev libxcb-image0-dev \
        libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev \
        libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev libxcb-xkb-dev \
        libxkbcommon-dev libxkbcommon-x11-dev \
    && rm -rf /var/lib/apt/lists/* \
    && python3 -m venv /opt/pytools \
    && /opt/pytools/bin/pip install --no-cache-dir \
        "aqtinstall @ git+https://github.com/miurahr/aqtinstall.git@${AQT_REF}" \
        "cmake==${CMAKE_PIP_VERSION}" \
    && ln -s /opt/pytools/bin/aqt /opt/pytools/bin/cmake /opt/pytools/bin/ctest /opt/pytools/bin/cpack /usr/local/bin/ \
    && cmake --version && aqt version
# NOTE: scripts are COPY'd as late as possible (per stage, one file at a time)
# so that editing build-app.sh / deploy.py never invalidates the cached
# multi-hour static Qt build layers.

# ── qt-host: prebuilt Linux Qt (host tools for cross builds, or the linux-shared target) ──
# Modules match what the Windows Qt installer pulls in for this project:
# qtgraphs (+ its optional qtquick3d dependency, which the prebuilt Graphs
# binaries are built with) and qtshadertools (qsb, needed by quick3d/graphs).
FROM base AS qt-host
ARG QT_VERSION
RUN aqt install-qt linux desktop "${QT_VERSION}" linux_gcc_64 -O /opt/qt \
        -m qtgraphs qtquick3d qtshadertools \
    && ln -s "/opt/qt/${QT_VERSION}/gcc_64" /opt/qt-host

# ── mingw: llvm-mingw cross compiler (Linux-hosted, targets Windows x86_64 UCRT) ──
# 20231128 = LLVM 17.0.6, the same llvm-mingw version Qt's official
# win64_llvm_mingw 6.11.1 binaries are built with (ABI match for libc++).
FROM qt-host AS mingw
ARG LLVM_MINGW_VERSION
RUN curl -fsSL "https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VERSION}/llvm-mingw-${LLVM_MINGW_VERSION}-ucrt-ubuntu-20.04-x86_64.tar.xz" \
        | tar -xJ -C /opt \
    && mv "/opt/llvm-mingw-${LLVM_MINGW_VERSION}-ucrt-ubuntu-20.04-x86_64" /opt/llvm-mingw
ENV PATH=/opt/llvm-mingw/bin:${PATH}
COPY scripts/toolchain-llvm-mingw.cmake /opt/fdbuild/

# ── toolchain variants ───────────────────────────────────────────────────────
FROM qt-host AS toolchain-linux-shared
RUN ln -s /opt/qt-host /opt/qt-target

FROM mingw AS toolchain-windows-shared
ARG QT_VERSION
RUN aqt install-qt windows desktop "${QT_VERSION}" win64_llvm_mingw -O /opt/qt \
        -m qtgraphs qtquick3d qtshadertools \
    && ln -s "/opt/qt/${QT_VERSION}/llvm-mingw_64" /opt/qt-target

FROM mingw AS toolchain-windows-static
ARG QT_VERSION
ARG QT_BUILD_JOBS=
COPY scripts/build-qt-static.sh /opt/fdbuild/
RUN sed -i 's/\r$//' /opt/fdbuild/build-qt-static.sh \
    && QT_VERSION="${QT_VERSION}" QT_BUILD_JOBS="${QT_BUILD_JOBS}" \
        bash /opt/fdbuild/build-qt-static.sh windows /opt/qt-static \
    && ln -s /opt/qt-static /opt/qt-target

FROM base AS toolchain-linux-static
ARG QT_VERSION
ARG QT_BUILD_JOBS=
ARG DEBIAN_FRONTEND=noninteractive
# The OpenSSL headers: without them, the static Qt has no TLS backend and the
# update check (#117, HTTPS) fails. With them, Qt's configure builds its
# OpenSSL backend (-openssl-runtime is the default: the app loads the
# libssl.so.3 of the system at run time, with the security updates of the
# distribution). In this stage only: a change in base, or in
# build-qt-static.sh, also rebuilds the static Windows Qt, which uses Schannel
# (the TLS of Windows) and does not need OpenSSL.
RUN apt-get update && apt-get install -y --no-install-recommends libssl-dev \
    && rm -rf /var/lib/apt/lists/*
COPY scripts/build-qt-static.sh /opt/fdbuild/
RUN sed -i 's/\r$//' /opt/fdbuild/build-qt-static.sh \
    && QT_VERSION="${QT_VERSION}" QT_BUILD_JOBS="${QT_BUILD_JOBS}" \
        bash /opt/fdbuild/build-qt-static.sh linux /opt/qt-static \
    && ln -s /opt/qt-static /opt/qt-target
# Stop here when the static Qt has no OpenSSL backend (libqopensslbackend.a,
# a plugin that Qt builds only with the OpenSSL headers), not with a "TLS
# initialization failed" when the app runs. A layer of its own, after the
# long Qt build.
RUN test -n "$(find /opt/qt-static -name 'libqopensslbackend*' -print -quit)" \
    || { echo "error: the static Qt has no OpenSSL TLS backend (libssl-dev missing?)" >&2; exit 1; }

# ── final: the variant selected in .env ──────────────────────────────────────
FROM toolchain-${TARGET}-${LINK_MODE} AS final
ARG TARGET
ARG LINK_MODE
ARG QT_VERSION
# Recorded so build-app.sh can refuse to run if .env changed but the image
# wasn't rebuilt.
ENV IMAGE_TARGET=${TARGET} \
    IMAGE_LINK_MODE=${LINK_MODE} \
    IMAGE_QT_VERSION=${QT_VERSION}
COPY scripts/ /opt/fdbuild/
# Tolerate a Windows checkout that converted the scripts to CRLF.
RUN find /opt/fdbuild -maxdepth 1 -type f -exec sed -i 's/\r$//' {} + \
    && chmod +x /opt/fdbuild/*.sh
WORKDIR /work
ENTRYPOINT ["/opt/fdbuild/build-app.sh"]
