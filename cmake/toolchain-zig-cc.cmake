# Cross-compile toolchain that uses Zig as the C / linker backend.
#
# Why Zig instead of musl-cross-make: GitHub Actions Azure IPs can't
# reach musl.cc. Zig ships its own clang fork plus pre-built libc
# (musl, glibc, mingw-w64 headers) for every supported target as one
# single tarball — pulled from ziglang.org / mirrors that GH actually
# can reach.
#
# Caller is expected to have created a small shim script in PATH that
# invokes `zig cc -target <triple>` (the workflow does this — see
# .github/workflows/release.yml). The shim lives at $AFM_ZIG_SHIM and
# its companion `*-cxx`, `*-ar`, `*-ranlib` siblings.
#
# Locally, the same can be done by hand:
#   echo '#!/bin/sh
#   exec zig cc -target aarch64-linux-musl "$@"' > /tmp/zig-cc
#   chmod +x /tmp/zig-cc
#   cmake -DCMAKE_C_COMPILER=/tmp/zig-cc \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-zig-cc.cmake ...

if(NOT DEFINED AFM_ZIG_TARGET AND DEFINED ENV{AFM_ZIG_TARGET})
    set(AFM_ZIG_TARGET "$ENV{AFM_ZIG_TARGET}")
endif()
if(NOT DEFINED AFM_ZIG_TARGET)
    message(FATAL_ERROR
        "Set -DAFM_ZIG_TARGET=<zig-triple> (e.g. aarch64-linux-musl) "
        "or export AFM_ZIG_TARGET=<triple> in the environment.")
endif()
set(AFM_ZIG_TARGET "${AFM_ZIG_TARGET}" CACHE STRING "zig cross target" FORCE)

# Architecture name from the triple — first chunk before the dash.
string(REGEX REPLACE "^([^-]+)-.*" "\\1" _AFM_ARCH "${AFM_ZIG_TARGET}")

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR ${_AFM_ARCH})

# CMake re-includes this file inside try_compile sub-projects, where
# -D vars from the outer cmake invocation aren't visible. Both compiler
# selection and the AFM_ZIG_TARGET cache entry above need to survive
# that re-include — environment fall-back covers both cases.
if(NOT DEFINED CMAKE_C_COMPILER AND DEFINED ENV{CC})
    set(CMAKE_C_COMPILER "$ENV{CC}")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER AND DEFINED ENV{CXX})
    set(CMAKE_CXX_COMPILER "$ENV{CXX}")
endif()
if(NOT DEFINED CMAKE_AR AND DEFINED ENV{AR})
    set(CMAKE_AR "$ENV{AR}" CACHE FILEPATH "ar")
endif()
if(NOT DEFINED CMAKE_RANLIB AND DEFINED ENV{RANLIB})
    set(CMAKE_RANLIB "$ENV{RANLIB}" CACHE FILEPATH "ranlib")
endif()

# Restrict find_*() to nothing host-side; libcurl auto-detects libidn2
# / zlib / etc and would otherwise yank `-isystem /usr/include` into a
# cross compile.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static link via clang/lld driver (zig cc); same flag as gcc.
# Add -ffunction-sections / -fdata-sections + --gc-sections so the
# linker drops every libcurl / wolfSSL routine we don't actually call
# (we only do an /answers.js GET — not the entire SSL stack reaches
# the final binary). Without these, zig binaries balloon to ~10 MB
# vs ~2.5 MB stripped + gc'd.
set(CMAKE_C_FLAGS_INIT          "-ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT        "-ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -Wl,--gc-sections")
