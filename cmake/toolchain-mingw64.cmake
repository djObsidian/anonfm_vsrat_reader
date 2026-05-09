set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# Pin the Windows API target to Win7 (NT 6.1). This is the lowest
# version for which wolfSSL's `XINET_PTON()` finds a matching symbol —
# `InetPton` (uppercase, the Vista+ Unicode variant) only exists in
# ws2_32 from NT 6.0 onwards. Older mingw-w64 packages (e.g. the one
# in Ubuntu 22.04) won't link without this define even though the
# compiler tolerates it. Win10/11 obviously also accept Win7 binaries.
add_compile_definitions(WINVER=0x0601 _WIN32_WINNT=0x0601)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}-ranlib)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Static libgcc/libstdc++ so the .exe is self-contained
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")
