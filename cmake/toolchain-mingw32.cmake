# Cross-compile for 32-bit Windows (i686) using mingw-w64.
#
# Why a separate i686 toolchain in addition to the x86_64 one:
#   * Windows XP (NT 5.1) and Server 2003 only have 32-bit kernels in the
#     wild. To run there at all the .exe must be PE32 (i386), not PE32+.
#   * Windows 7 32-bit installations still exist on netbooks / embedded
#     industrial gear. A 32-bit binary runs on both 32- and 64-bit Win7.
#
# We pin the Windows API target to NT 5.1 (XP / Server 2003) and the
# subsystem version on the PE header to match. mingw-w64 calls into a few
# Vista+ APIs by default; everything we use (GetStdHandle, SetConsoleMode,
# SetConsoleOutputCP, SetConsoleTextAttribute) is XP-era. Virtual-terminal
# processing on the console isn't available on Win7 — render.c falls back
# to SetConsoleTextAttribute via the helpers in platform.c, so colours
# work on legacy CMD too.

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_SYSTEM_VERSION   5.1)   # XP / Server 2003 era

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

# Pin to XP-era Windows API headers so we don't accidentally call a
# Vista+ symbol that won't resolve at load time on the target.
add_compile_definitions(WINVER=0x0501 _WIN32_WINNT=0x0501)

# PE header SUBSYSTEM version 5.01 = "this exe runs on XP". Without this
# the loader on XP refuses with "not a valid Win32 application".
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-static -static-libgcc -static-libstdc++ -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,1 -Wl,--major-os-version,5 -Wl,--minor-os-version,1")
