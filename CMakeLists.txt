cmake_minimum_required(VERSION 3.20)
project(anonfm_vsrat_reader C)

# Hardcore ANSI C as the user requested. Apply only to OUR sources, not deps.
set(AFM_C_FLAGS_STRICT
    -std=c89
    -pedantic
    -Wall
    -D_POSIX_C_SOURCE=200809L
)

if (MSVC)
	# list(APPEND AFM_C_FLAGS_STRICT -W4)
	list(APPEND AFM_C_FLAGS_STRICT /wd4711 /wd4710)
else()
    list(APPEND AFM_C_FLAGS_STRICT
        -Wextra
        -Wno-long-long
        -Wno-overlength-strings
    )
endif()

# wolfSSL's constant-time HMAC trips a false-positive -Werror=stringop-overflow
# under newer gcc. Drop -Werror project-wide; our own target uses an explicit
# warning set without -Werror anyway.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-error)
endif()

include(FetchContent)

# ----- wolfSSL (static) ---------------------------------------------------
# Prefer a pre-staged copy under vendor/wolfssl (lets us build on hosts
# without internet access, e.g. WSL with broken DNS). Falls back to git.
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/wolfssl/CMakeLists.txt")
    FetchContent_Declare(wolfssl SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/wolfssl")
else()
    FetchContent_Declare(wolfssl
        GIT_REPOSITORY https://github.com/wolfSSL/wolfssl.git
        GIT_TAG        v5.7.6-stable
        GIT_SHALLOW    TRUE
    )
endif()
set(BUILD_SHARED_LIBS    OFF CACHE BOOL "" FORCE)
set(WOLFSSL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(WOLFSSL_CRYPTTESTS   OFF CACHE BOOL "" FORCE)
set(WOLFSSL_OPENSSLEXTRA ON  CACHE BOOL "" FORCE)
set(WOLFSSL_OPENSSLALL   ON  CACHE BOOL "" FORCE)
set(WOLFSSL_TLSV13       ON  CACHE BOOL "" FORCE)
set(WOLFSSL_SNI          ON  CACHE BOOL "" FORCE)
set(WOLFSSL_ALPN         ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(wolfssl)

# ----- libcurl (static, HTTPS-only, on wolfSSL) ---------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/curl/CMakeLists.txt")
    FetchContent_Declare(curl SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/curl")
else()
    FetchContent_Declare(curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG        curl-8_11_0
        GIT_SHALLOW    TRUE
    )
endif()
set(BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE     OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING      OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_TESTS ON  CACHE BOOL "" FORCE)
set(HTTP_ONLY          ON  CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL   OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2   OFF CACHE BOOL "" FORCE)
set(CURL_USE_WOLFSSL   ON  CACHE BOOL "" FORCE)
set(CURL_WOLFSSL       ON  CACHE BOOL "" FORCE)
# Turn off every optional curl dep that auto-detects from the host. When
# cross-compiling with a musl-cross toolchain whose sysroot lacks idn2 /
# zlib / psl / etc, libcurl's CMake "helpfully" finds the BUILD HOST's
# copies in /usr/include, drags `-isystem /usr/include` into the flags,
# and the cross compile dies inside <features-time64.h>. We don't need
# any of these for an /answers.js GET — kill them with fire.
set(CURL_USE_LIBIDN2   OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL    OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB          OFF CACHE STRING "" FORCE)
set(CURL_BROTLI        OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD          OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2        OFF CACHE BOOL "" FORCE)
set(USE_LIBRTMP        OFF CACHE BOOL "" FORCE)
set(USE_NGTCP2         OFF CACHE BOOL "" FORCE)
set(USE_QUICHE         OFF CACHE BOOL "" FORCE)
set(USE_OPENLDAP       OFF CACHE BOOL "" FORCE)
set(ENABLE_UNIX_SOCKETS ON CACHE BOOL "" FORCE)
set(CURL_USE_PKGCONFIG OFF CACHE BOOL "" FORCE)
# Point libcurl at the FetchContent'd wolfSSL build/install
set(WOLFSSL_INCLUDE_DIR "${wolfssl_SOURCE_DIR}" CACHE PATH "" FORCE)
set(WOLFSSL_LIBRARY     wolfssl                CACHE STRING "" FORCE)
FetchContent_MakeAvailable(curl)

# ----- bake Mozilla CA bundle into the binary ----------------------------
# wolfSSL has no default trust store on Windows; we embed cacert.pem and
# pass it via CURLOPT_CAINFO_BLOB on every TLS handle. Same pem on Linux too,
# so behaviour is identical across platforms.
set(AFM_CACERT_SRC "${CMAKE_SOURCE_DIR}/vendor/cacert.pem")
set(AFM_CACERT_C   "${CMAKE_BINARY_DIR}/cacert_data.c")
if(NOT EXISTS "${AFM_CACERT_SRC}")
    message(FATAL_ERROR "vendor/cacert.pem missing — fetch it from https://curl.se/ca/cacert.pem")
endif()
file(READ "${AFM_CACERT_SRC}" AFM_CACERT_HEX HEX)
string(REGEX REPLACE "(..)" "0x\\1," AFM_CACERT_BYTES "${AFM_CACERT_HEX}")
file(WRITE "${AFM_CACERT_C}"
"/* Auto-generated from vendor/cacert.pem — do not edit. */\n"
"const unsigned char afm_cacert_data[] = {\n${AFM_CACERT_BYTES}\n};\n"
"const unsigned long afm_cacert_size = sizeof(afm_cacert_data);\n")

# ----- our binary ---------------------------------------------------------
add_executable(anonfm_vsrat_reader
    src/main.c
    src/http.c
    src/parse.c
    src/html.c
    src/colors.c
    src/render.c
    src/seen.c
    src/platform.c
    "${AFM_CACERT_C}"
)

target_include_directories(anonfm_vsrat_reader PRIVATE
    src
    third_party
    ${curl_SOURCE_DIR}/include
)

target_compile_options(anonfm_vsrat_reader PRIVATE ${AFM_C_FLAGS_STRICT})

# Static link both
target_link_libraries(anonfm_vsrat_reader PRIVATE libcurl wolfssl)

if(WIN32)
    target_link_libraries(anonfm_vsrat_reader PRIVATE
        ws2_32 crypt32 bcrypt advapi32 user32 kernel32
    )
    # libcurl needs CURL_STATICLIB symbol when linking statically
    target_compile_definitions(anonfm_vsrat_reader PRIVATE CURL_STATICLIB)
endif()

set_target_properties(anonfm_vsrat_reader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
