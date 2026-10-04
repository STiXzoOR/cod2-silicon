#!/bin/bash
# Shared source-build prerequisites and the app build.
check_prerequisites() {
    if [[ $(/usr/bin/uname -s) != Darwin || $(/usr/bin/uname -m) != arm64 ]]; then
        printf 'CoD2 Silicon requires an Apple silicon Mac running macOS 13 or later.\n' >&2
        return 1
    fi
    local os_version
    os_version=$(/usr/bin/sw_vers -productVersion)
    if [[ ${os_version%%.*} -lt 13 ]]; then
        printf 'CoD2 Silicon requires macOS 13 or later; this Mac runs %s.\n' "$os_version" >&2
        return 1
    fi
    if ! /usr/bin/xcode-select -p >/dev/null 2>&1 || ! /usr/bin/xcrun --find clang >/dev/null 2>&1 || ! /usr/bin/xcrun --find swiftc >/dev/null 2>&1; then
        printf 'Install the Xcode Command Line Tools: xcode-select --install\n' >&2
        return 1
    fi
    if ! command -v cmake >/dev/null 2>&1; then
        printf 'Install CMake from https://cmake.org/download/ (add its bin directory to PATH), or run: brew install cmake\n' >&2
        return 1
    fi
    local swift_major
    swift_major=$(/usr/bin/xcrun swiftc --version | /usr/bin/sed -n 's/.*Swift version \([0-9][0-9]*\).*/\1/p' | /usr/bin/head -n 1)
    if [[ -z "$swift_major" || "$swift_major" -lt 6 ]]; then
        printf 'Source builds need Swift 6 (Xcode 16 or later). The prebuilt app runs on macOS 13 or later. Update your developer tools.\n' >&2
        return 1
    fi
    python=$(/usr/bin/xcrun --find python3)
    if ! "$python" -c 'import sys; sys.exit(sys.version_info < (3, 9))'; then
        printf 'The build needs Python 3.9 or later from the current Xcode Command Line Tools. Update the tools with: xcode-select --install\n' >&2
        return 1
    fi
}

# Full speed by default; COD2_BUILD_BACKGROUND=1 keeps builds on efficiency cores.
run_build() {
    if [[ ${COD2_BUILD_BACKGROUND:-0} == 1 ]]; then
        /usr/sbin/taskpolicy -b /usr/bin/nice -n 19 "$@"
    else
        "$@"
    fi
}

build_app() {
    local root=$1 build=$2 app=$3 version=$4 mac_binary=$5 stabs_binary=$6
    check_prerequisites
    # The committed build/lp64_gen snapshot needs no private input. When both
    # reference binaries are supplied, CMake regenerates and verifies it instead.
    local datagen_args=()
    if [[ -n "$mac_binary" || -n "$stabs_binary" ]]; then
        if [[ ! -f "$mac_binary" || ! -f "$stabs_binary" ]]; then
            printf 'Snapshot verification needs both --mac-binary and --stabs-binary as existing files.\nOmit both to build from the committed snapshot.\n' >&2
            return 1
        fi
        datagen_args=("-DCOD2_REGENERATE_TYPED_DATA=ON" "-DCOD2_STABS_BINARY=$stabs_binary" "-DCOD2_VALUES_BINARY=$mac_binary")
    else
        datagen_args=("-DCOD2_REGENERATE_TYPED_DATA=OFF" "-DCOD2_STABS_BINARY=" "-DCOD2_VALUES_BINARY=")
    fi
    "$root/scripts/build-sdl.sh" "$build/deps"
    local prefix="$build/deps/install" sdk
    sdk=$(/usr/bin/xcrun --show-sdk-path)
    run_build cmake -S "$root" -B "$build/client" \
        -DCOD2_X64=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 "-DCMAKE_OSX_SYSROOT=$sdk" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_MACOS_RELEASE=ON "-DCOD2_MACOS_SDL_PREFIX=$prefix" \
        "-DCMAKE_PREFIX_PATH=$prefix" "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local" \
        "-DZLIB_LIBRARY=$sdk/usr/lib/libz.tbd" "-DZLIB_INCLUDE_DIR=$sdk/usr/include" \
        "-DCOD2_CURL_LIBRARY=$sdk/usr/lib/libcurl.tbd" \
        "${datagen_args[@]}" "-DCOD2_TYPED_DATA_DIR=$build/typed" "-DPython3_EXECUTABLE=$python"
    run_build cmake --build "$build/client" --target cod2_macos --parallel "$(/usr/sbin/sysctl -n hw.ncpu)"
    run_build cmake --build "$build/client" --target cod2_launcher --parallel 3
    run_build "$python" "$root/tools/cod2x/make_macos_app.py" \
        "$build/client/cod2_macos" "$app" --replace --launcher "$build/client/launcher/CoD2Launcher" --frameworks "$prefix" --version "$version"
}
