# CMake toolchain file: cross-compile for Windows x86_64 from the Linux build
# container, using llvm-mingw (https://github.com/mstorsjo/llvm-mingw) installed
# at /opt/llvm-mingw. Used both to build static Qt from source
# (build-qt-static.sh) and to build FinanceDashboard itself (build-app.sh).
#
# Standard CMake cross-compiling variables — see
# https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-windows

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_triple x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${_triple}-clang)
set(CMAKE_CXX_COMPILER ${_triple}-clang++)
set(CMAKE_RC_COMPILER  ${_triple}-windres)
set(CMAKE_AR           /opt/llvm-mingw/bin/llvm-ar)
set(CMAKE_RANLIB       /opt/llvm-mingw/bin/llvm-ranlib)

# Target-side search roots: the mingw sysroot, plus the Qt prefix the app is
# compiled against (/opt/qt-target — exists once the Qt stage has run; listing
# a not-yet-existing path is harmless). Host tools are added by Qt itself from
# QT_HOST_PATH.
list(APPEND CMAKE_FIND_ROOT_PATH /opt/llvm-mingw/${_triple} /opt/qt-target)

# Programs (moc, qmlcachegen, …) must come from the host; libraries/headers
# from the target roots only; packages from both, so CMAKE_PREFIX_PATH /
# QT_HOST_PATH entries outside the roots still resolve.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
