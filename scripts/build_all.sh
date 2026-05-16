#!/usr/bin/env bash
# Build anonfm_vsrat_reader for every target we have a toolchain for.
#
# Inspired by how Tor and hysteria ship: one tarball per (arch, libc) pair,
# each statically linked, dropped into ./dist/.
#
# Toolchains required (install whichever subset you care about):
#   * mingw-w64               — Windows .exe (x86_64 + i686)
#   * musl-cross-make / musl.cc bundles for each Linux target
#   * Android NDK             — Android (bionic) targets; export
#                               ANDROID_NDK_ROOT or ANDROID_NDK_HOME
#                               pointing at the NDK install dir.
#   * zig                     — alternative for the Linux musl targets
#                               above (see toolchain-zig-cc.cmake); not
#                               used by this script.
#
# Skips a target silently if its compiler isn't on PATH. Add or remove
# entries in TARGETS to taste — every entry is "<label>:<extra args>".

set -u  # leave -e off so one broken target doesn't abort the whole batch

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$ROOT/dist"
mkdir -p "$DIST"

# ----------------------------------------------------------------------
# Windows targets (mingw-w64)
# ----------------------------------------------------------------------
WIN_TARGETS=(
    "win64:cmake/toolchain-mingw64.cmake:x86_64-w64-mingw32-gcc"
    "win32-xp:cmake/toolchain-mingw32.cmake:i686-w64-mingw32-gcc"
)

# ----------------------------------------------------------------------
# Linux targets (musl-cross). label:triple:probe-binary
# Comment out lines you don't have toolchains for.
# ----------------------------------------------------------------------
LINUX_TARGETS=(
    "linux-x86_64:x86_64-linux-musl:x86_64-linux-musl-gcc"
    "linux-i686:i686-linux-musl:i686-linux-musl-gcc"
    "linux-aarch64:aarch64-linux-musl:aarch64-linux-musl-gcc"
    "linux-armv7:armv7l-linux-musleabihf:armv7l-linux-musleabihf-gcc"
    "linux-armv6:armv6-linux-musleabihf:armv6-linux-musleabihf-gcc"
    "linux-armv5:armv5l-linux-musleabi:armv5l-linux-musleabi-gcc"
    "linux-mips:mips-linux-musl:mips-linux-musl-gcc"
    "linux-mipsel:mipsel-linux-musl:mipsel-linux-musl-gcc"
    "linux-mips64:mips64-linux-musl:mips64-linux-musl-gcc"
    "linux-mips64el:mips64el-linux-musl:mips64el-linux-musl-gcc"
    "linux-powerpc:powerpc-linux-musl:powerpc-linux-musl-gcc"
    "linux-ppc64le:powerpc64le-linux-musl:powerpc64le-linux-musl-gcc"
    "linux-riscv64:riscv64-linux-musl:riscv64-linux-musl-gcc"
    "linux-s390x:s390x-linux-musl:s390x-linux-musl-gcc"
)

# ----------------------------------------------------------------------
# Android targets (bionic via NDK). label:android-abi[:api]
#
# Bionic ABI, dynamic-linked against libc.so/libm.so/libdl.so which are
# present on every Android device. Output is a normal Android ELF that
# runs in Termux, adb shell, or from any path the app sandbox allows.
#
# Requires $ANDROID_NDK_ROOT (or $ANDROID_NDK_HOME) to point at an NDK
# install. Download from https://developer.android.com/ndk. r25 or newer
# is fine — we don't use any bleeding-edge libc API.
#
# API level default is 24 (Android 7.0 Nougat, 99%+ device coverage in
# 2026). Override per entry by appending after the ABI.
# ----------------------------------------------------------------------
ANDROID_TARGETS=(
    "android-aarch64:arm64-v8a:24"
    "android-armv7a:armeabi-v7a:24"
    "android-x86_64:x86_64:24"
)

OK=()
SKIP=()
FAIL=()

build_win() {
    local label="$1" toolchain="$2" probe="$3"
    if ! command -v "$probe" >/dev/null 2>&1; then
        SKIP+=("$label (no $probe)")
        return
    fi
    local build_dir="build-$label"
    rm -rf "$build_dir"
    if cmake -S "$ROOT" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_TOOLCHAIN_FILE="$toolchain" >/dev/null 2>&1 \
       && cmake --build "$build_dir" -j"$(nproc)" >/dev/null 2>&1; then
        cp "$build_dir/anonfm_vsrat_reader.exe" "$DIST/anonfm_vsrat_reader-$label.exe"
        OK+=("$label")
    else
        FAIL+=("$label")
    fi
}

build_linux() {
    local label="$1" triple="$2" probe="$3"
    if ! command -v "$probe" >/dev/null 2>&1; then
        SKIP+=("$label (no $probe)")
        return
    fi
    local build_dir="build-$label"
    rm -rf "$build_dir"
    if cmake -S "$ROOT" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-musl-cross.cmake \
             -DAFM_MUSL_TARGET="$triple" >/dev/null 2>&1 \
       && cmake --build "$build_dir" -j"$(nproc)" >/dev/null 2>&1; then
        cp "$build_dir/anonfm_vsrat_reader" "$DIST/anonfm_vsrat_reader-$label"
        # strip is best-effort; some embedded triples ship no strip wrapper
        "${triple}-strip" "$DIST/anonfm_vsrat_reader-$label" 2>/dev/null || true
        OK+=("$label")
    else
        FAIL+=("$label")
    fi
}

# Android builds use NDK clang via toolchain-android-ndk.cmake. We probe
# for $ANDROID_NDK_ROOT (or _HOME, or _NDK) — the NDK ships its own
# clang/llvm-ar/llvm-ranlib in toolchains/llvm/prebuilt/<host>/bin/.
build_android() {
    local label="$1" abi="$2" api="$3"
    local ndk="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-${ANDROID_NDK:-}}}"
    if [ -z "$ndk" ] || [ ! -d "$ndk" ]; then
        SKIP+=("$label (no ANDROID_NDK_ROOT)")
        return
    fi
    local build_dir="build-$label"
    rm -rf "$build_dir"
    if cmake -S "$ROOT" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-android-ndk.cmake \
             -DAFM_ANDROID_NDK="$ndk" \
             -DAFM_ANDROID_ABI="$abi" \
             -DAFM_ANDROID_API="$api" >/dev/null 2>&1 \
       && cmake --build "$build_dir" -j"$(nproc)" >/dev/null 2>&1; then
        cp "$build_dir/anonfm_vsrat_reader" "$DIST/anonfm_vsrat_reader-$label"
        OK+=("$label")
    else
        FAIL+=("$label")
    fi
}

echo "==> Windows targets"
for entry in "${WIN_TARGETS[@]}"; do
    IFS=':' read -r label toolchain probe <<<"$entry"
    echo "  $label..."
    build_win "$label" "$toolchain" "$probe"
done

echo "==> Linux targets (musl-static)"
for entry in "${LINUX_TARGETS[@]}"; do
    IFS=':' read -r label triple probe <<<"$entry"
    echo "  $label..."
    build_linux "$label" "$triple" "$probe"
done

echo "==> Android targets (bionic via NDK)"
for entry in "${ANDROID_TARGETS[@]}"; do
    IFS=':' read -r label abi api <<<"$entry"
    echo "  $label..."
    build_android "$label" "$abi" "$api"
done

echo
echo "===== Summary ====="
echo "OK:    ${#OK[@]}"
for t in "${OK[@]}";   do echo "  + $t"; done
echo "SKIP:  ${#SKIP[@]}"
for t in "${SKIP[@]}"; do echo "  - $t"; done
echo "FAIL:  ${#FAIL[@]}"
for t in "${FAIL[@]}"; do echo "  ! $t"; done

[ "${#FAIL[@]}" -eq 0 ]
