# Cross-compile for 32-bit Windows (i686) using mingw-w64.
#
# Targets Windows 7 32-bit and up. We tried XP (NT 5.1) originally,
# but wolfSSL hard-codes a call to `InetPton` (the Vista+ Unicode-aware
# wrapper in ws2_32) which simply does not exist in XP-era ws2_32.dll.
# Patching wolfSSL to fall back to inet_addr would mean shaving the SSL
# stack down — out of scope. So XP is dropped, the smallest supported
# Windows is now 7-32-bit (still useful for old netbooks / embedded
# industrial gear running 32-bit Win7 / 8.1).
#
# A 32-bit binary still runs on both 32- and 64-bit Win7+, so this also
# serves users who want a smaller .exe on x64 Windows.

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_SYSTEM_VERSION   6.1)   # Win7 / Server 2008 R2 era

set(TOOLCHAIN_PREFIX i686-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}-ranlib)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Pin the Windows API target to NT 6.1 (Win7). See comment in
# toolchain-mingw64.cmake — wolfSSL's InetPton call needs >= 0x0600.
add_compile_definitions(WINVER=0x0601 _WIN32_WINNT=0x0601)

# PE header SUBSYSTEM version 6.01 = "this exe runs on Win7+".
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-static -static-libgcc -static-libstdc++ -Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1 -Wl,--major-os-version,6 -Wl,--minor-os-version,1")
