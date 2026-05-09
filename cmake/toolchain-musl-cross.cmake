# Generic musl-cross-make / zig-cc style cross-compile toolchain.
#
# Drop-in for the popular pre-built musl toolchain bundles
# (https://musl.cc, https://github.com/richfelker/musl-cross-make).
# Pick a target by passing -DAFM_MUSL_TARGET=<triple>, e.g.:
#
#   cmake -S . -B build-aarch64 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-musl-cross.cmake \
#         -DAFM_MUSL_TARGET=aarch64-linux-musl
#
# Supported triples (what musl.cc ships):
#   x86_64-linux-musl
#   i686-linux-musl
#   aarch64-linux-musl
#   armv7l-linux-musleabihf
#   armv6-linux-musleabihf       (Raspberry Pi Zero / 1)
#   armv5l-linux-musleabi        (ancient ARM, NSLU2-class)
#   mips-linux-musl              (big-endian MIPS, e.g. routers)
#   mipsel-linux-musl            (little-endian MIPS)
#   mips64-linux-musl
#   mips64el-linux-musl
#   powerpc-linux-musl
#   powerpc64le-linux-musl
#   riscv64-linux-musl
#   s390x-linux-musl
#   m68k-linux-musl              (Amiga, Atari, old Suns)
#   microblaze-linux-musl
#   sh4-linux-musl               (SEGA Dreamcast-class)
#
# We assume the toolchain is on PATH as <triple>-gcc / <triple>-ld etc.
# Set AFM_MUSL_PREFIX to override the location, e.g. /opt/cross/aarch64.

# CMake re-includes the toolchain file inside try_compile sub-projects,
# where -D cache vars from the outer invocation are NOT visible. So we
# also accept AFM_MUSL_TARGET via the environment and cache it back so
# subsequent re-includes see the same value.
if(NOT DEFINED AFM_MUSL_TARGET AND DEFINED ENV{AFM_MUSL_TARGET})
    set(AFM_MUSL_TARGET "$ENV{AFM_MUSL_TARGET}")
endif()
if(NOT DEFINED AFM_MUSL_TARGET)
    message(FATAL_ERROR
        "Set -DAFM_MUSL_TARGET=<triple> (e.g. aarch64-linux-musl) "
        "or export AFM_MUSL_TARGET=<triple> in the environment.")
endif()
set(AFM_MUSL_TARGET "${AFM_MUSL_TARGET}" CACHE STRING "musl-cross target triple" FORCE)
if(DEFINED AFM_MUSL_PREFIX)
    set(AFM_MUSL_PREFIX "${AFM_MUSL_PREFIX}" CACHE PATH "musl-cross install prefix" FORCE)
elseif(DEFINED ENV{AFM_MUSL_PREFIX})
    set(AFM_MUSL_PREFIX "$ENV{AFM_MUSL_PREFIX}" CACHE PATH "musl-cross install prefix" FORCE)
endif()

string(REGEX REPLACE "^([^-]+)-.*" "\\1" AFM_MUSL_ARCH "${AFM_MUSL_TARGET}")

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR ${AFM_MUSL_ARCH})

if(DEFINED AFM_MUSL_PREFIX)
    set(_PREFIX "${AFM_MUSL_PREFIX}/bin/${AFM_MUSL_TARGET}-")
else()
    set(_PREFIX "${AFM_MUSL_TARGET}-")
endif()

set(CMAKE_C_COMPILER   ${_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${_PREFIX}g++)
set(CMAKE_AR           ${_PREFIX}ar)
set(CMAKE_RANLIB       ${_PREFIX}ranlib)
set(CMAKE_STRIP        ${_PREFIX}strip)

# Anchor find_*() probes to the cross-toolchain's sysroot. Without this,
# libcurl's auto-detection of optional libs (libidn2, libpsl, zlib, ...)
# happily picks up the host's /usr/include and /usr/lib, ending up with
# `-isystem /usr/include` in our cross compile and an inevitable explosion
# in glibc-only headers like <features-time64.h>.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-sysroot
    OUTPUT_VARIABLE _AFM_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(_AFM_SYSROOT AND EXISTS "${_AFM_SYSROOT}")
    set(CMAKE_SYSROOT       "${_AFM_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${_AFM_SYSROOT}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static everything — the whole point of musl-cross is single-binary
# portability across glibc-version chaos.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
