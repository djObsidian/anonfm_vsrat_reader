# Cross-compile toolchain for Android (bionic libc) via the Android NDK.
#
# Why not zig cc? Zig 0.13 doesn't ship a bionic libc — its bundled libc
# kits are musl, glibc, mingw-w64 and macOS only. Targeting
# `-target *-linux-android` with stock zig fails at link time with
# "libc not available". The clean answer is to use the NDK directly,
# which ships sysroots for every API level out of the box.
#
# Why not the NDK's own android.toolchain.cmake? It works, but pulls in
# a bunch of magic CMake variables (ANDROID_STL, ANDROID_LD,
# ANDROID_ARM_NEON, ...) that are irrelevant for a single-file pure C
# binary. A 40-line direct-invocation toolchain is easier to reason
# about and matches the style of toolchain-mingw64.cmake.
#
# Usage:
#   export ANDROID_NDK_ROOT=/path/to/ndk     # or ANDROID_NDK_HOME
#   cmake -S . -B build-android-aarch64 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-android-ndk.cmake \
#         -DAFM_ANDROID_ABI=arm64-v8a \
#         -DAFM_ANDROID_API=24
#
# Supported ABIs (the four official NDK ones):
#   arm64-v8a       (default, aarch64) — 95%+ of devices in 2026
#   armeabi-v7a     (32-bit ARM, cortex-a-class) — 5% legacy
#   x86_64          (emulator + Chromebook + some industrial)
#   x86             (ancient emulator, rarely worth shipping)
#
# Default API level is 24 (Android 7.0 Nougat, Aug 2016) which covers
# 99%+ of currently-active devices. Drop to 21 (Lollipop) for 2014
# phones, or bump to 26 (Oreo) if you want to leverage newer libc APIs.

# ---- NDK location --------------------------------------------------------
if(NOT DEFINED AFM_ANDROID_NDK)
    if(DEFINED ENV{ANDROID_NDK_ROOT})
        set(AFM_ANDROID_NDK "$ENV{ANDROID_NDK_ROOT}")
    elseif(DEFINED ENV{ANDROID_NDK_HOME})
        set(AFM_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
    elseif(DEFINED ENV{ANDROID_NDK})
        set(AFM_ANDROID_NDK "$ENV{ANDROID_NDK}")
    endif()
endif()
if(NOT AFM_ANDROID_NDK OR NOT EXISTS "${AFM_ANDROID_NDK}")
    message(FATAL_ERROR
        "Android NDK not found. Set ANDROID_NDK_ROOT (or ANDROID_NDK_HOME) "
        "in the environment, or pass -DAFM_ANDROID_NDK=/path/to/ndk on the "
        "cmake command line. Download from https://developer.android.com/ndk.")
endif()
set(AFM_ANDROID_NDK "${AFM_ANDROID_NDK}" CACHE PATH "Android NDK root" FORCE)

# ---- ABI & API level -----------------------------------------------------
if(NOT DEFINED AFM_ANDROID_ABI AND DEFINED ENV{AFM_ANDROID_ABI})
    set(AFM_ANDROID_ABI "$ENV{AFM_ANDROID_ABI}")
endif()
if(NOT DEFINED AFM_ANDROID_ABI)
    set(AFM_ANDROID_ABI "arm64-v8a")
endif()
set(AFM_ANDROID_ABI "${AFM_ANDROID_ABI}" CACHE STRING "Android ABI" FORCE)

if(NOT DEFINED AFM_ANDROID_API AND DEFINED ENV{AFM_ANDROID_API})
    set(AFM_ANDROID_API "$ENV{AFM_ANDROID_API}")
endif()
if(NOT DEFINED AFM_ANDROID_API)
    set(AFM_ANDROID_API 24)
endif()
set(AFM_ANDROID_API "${AFM_ANDROID_API}" CACHE STRING "Android API level" FORCE)

# ABI → (clang triple prefix, CMAKE_SYSTEM_PROCESSOR).
# The NDK names clang wrappers as `<triple><api>-clang`, where <triple>
# follows the standard ABI names — note `armv7a-linux-androideabi` for
# armeabi-v7a (the "a" suffix on armv7), and `aarch64-linux-android` for
# arm64-v8a (no "a" suffix).
if(AFM_ANDROID_ABI STREQUAL "arm64-v8a")
    set(_AFM_TRIPLE "aarch64-linux-android")
    set(_AFM_PROC   "aarch64")
elseif(AFM_ANDROID_ABI STREQUAL "armeabi-v7a")
    set(_AFM_TRIPLE "armv7a-linux-androideabi")
    set(_AFM_PROC   "armv7-a")
elseif(AFM_ANDROID_ABI STREQUAL "x86_64")
    set(_AFM_TRIPLE "x86_64-linux-android")
    set(_AFM_PROC   "x86_64")
elseif(AFM_ANDROID_ABI STREQUAL "x86")
    set(_AFM_TRIPLE "i686-linux-android")
    set(_AFM_PROC   "i686")
else()
    message(FATAL_ERROR
        "Unknown AFM_ANDROID_ABI='${AFM_ANDROID_ABI}'. "
        "Valid: arm64-v8a, armeabi-v7a, x86_64, x86.")
endif()

# ---- Host detection ------------------------------------------------------
# NDK prebuilts ship for linux-x86_64, darwin-x86_64, windows-x86_64.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(_AFM_HOST_TAG "linux-x86_64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(_AFM_HOST_TAG "darwin-x86_64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(_AFM_HOST_TAG "windows-x86_64")
else()
    message(FATAL_ERROR
        "Unsupported build host '${CMAKE_HOST_SYSTEM_NAME}' — NDK only "
        "ships prebuilts for Linux, macOS, and Windows x86_64.")
endif()

set(_AFM_NDK_BIN
    "${AFM_ANDROID_NDK}/toolchains/llvm/prebuilt/${_AFM_HOST_TAG}/bin")

# Same CMAKE_SYSTEM_NAME=Linux trick used by toolchain-zig-cc.cmake:
# stay out of CMake's NDK-integration code path (which expects to drive
# ndk-build / the NDK's own toolchain), let -target on the clang invo-
# cation set bionic ABI and __ANDROID__ instead.
set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR ${_AFM_PROC})

# ---- Compiler & tools ----------------------------------------------------
# NDK clang wrappers encode (triple, API) in the binary name. Each one
# is a small shell script that calls the underlying `clang` with the
# right --target=<triple><api> and --sysroot=<ndk-sysroot>. We could
# call the bare clang and pass --target ourselves, but the wrappers
# also forward --gcc-toolchain etc — easier to just use them.
set(CMAKE_C_COMPILER   "${_AFM_NDK_BIN}/${_AFM_TRIPLE}${AFM_ANDROID_API}-clang")
set(CMAKE_CXX_COMPILER "${_AFM_NDK_BIN}/${_AFM_TRIPLE}${AFM_ANDROID_API}-clang++")
set(CMAKE_AR           "${_AFM_NDK_BIN}/llvm-ar")
set(CMAKE_RANLIB       "${_AFM_NDK_BIN}/llvm-ranlib")
set(CMAKE_STRIP        "${_AFM_NDK_BIN}/llvm-strip")

# Anchor find_*() to the NDK sysroot. Without this, libcurl's optional-
# dep auto-detection (libidn2, zlib, libpsl, ...) cheerfully reaches
# into the build host's /usr/include and yanks `-isystem /usr/include`
# into the cross compile, then dies inside glibc-only headers.
set(CMAKE_SYSROOT
    "${AFM_ANDROID_NDK}/toolchains/llvm/prebuilt/${_AFM_HOST_TAG}/sysroot")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---- Flags ---------------------------------------------------------------
# CRITICAL: no `-static`. Bionic has no static libc; static link would
# fail with `undefined reference to __libc_init` etc. We dynamic-link
# against libc.so / libm.so / libdl.so which are present on every
# Android device. The final ELF is ~1.5 MB (vs ~2.5 MB for the
# musl-static equivalent) and has zero runtime dependencies beyond
# what ships on every phone.
#
# Same gc-sections + linker-strip trick as the other toolchains to
# drop the wolfSSL/libcurl routines we don't reach.
set(CMAKE_C_FLAGS_INIT          "-ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT        "-ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -Wl,-s")

# PIE is mandatory on Android API 21+ for executables. NDK clang
# defaults to PIE for android targets, but be explicit.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
