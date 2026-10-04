#!/bin/bash
# Shared source-build prerequisites and licensed input discovery.
check_prerequisites() {
    if [[ $(/usr/bin/uname -s) != Darwin || $(/usr/bin/uname -m) != arm64 ]]; then
        printf 'CoD2 Silicon requires an Apple silicon Mac running macOS 13 or later.\n' >&2
        return 1
    fi
    if ! /usr/bin/xcode-select -p >/dev/null 2>&1 || ! /usr/bin/xcrun --find clang >/dev/null 2>&1; then
        printf 'Install the Xcode Command Line Tools: xcode-select --install\n' >&2
        return 1
    fi
    if ! command -v cmake >/dev/null 2>&1; then
        printf 'Install CMake from https://cmake.org/download/ (add its bin directory to PATH), or run: brew install cmake\n' >&2
        return 1
    fi
    python=$(/usr/bin/xcrun --find python3)
    if ! "$python" -c 'import sys; sys.exit(sys.version_info < (3, 9))'; then
        printf 'The build needs Python 3.9 or later from the current Xcode Command Line Tools. Update the tools with: xcode-select --install\n' >&2
        return 1
    fi
}

find_mac_binary() {
    local candidate base
    local roots=("$HOME/Games/CoD2-mac-bin" "$HOME/Games/CoD2"
        "$HOME/Library/Application Support/Steam/steamapps/common/Call of Duty 2")
    local libraries="$HOME/Library/Application Support/Steam/steamapps/libraryfolders.vdf"
    if [[ -f "$libraries" ]]; then
        while IFS= read -r base; do
            roots+=("$base/steamapps/common/Call of Duty 2")
        done < <(/usr/bin/awk -F '"' '$2 == "path" {gsub(/\\\\/, "\\", $4); print $4}' "$libraries")
    fi
    for base in /Applications/Call\ of\ Duty\ 2*; do
        [[ -d "$base" ]] && roots+=("$base")
    done
    for base in "${roots[@]}"; do
        [[ -d "$base" ]] || continue
        while IFS= read -r -d '' candidate; do
            printf '%s\n' "$candidate"
            return 0
        done < <(/usr/bin/find "$base" -type f -name 'Call of Duty 2 Multiplayer' -print0)
    done
    return 1
}

build_app() {
    local root=$1 build=$2 app=$3 version=$4 mac_binary=$5 stabs_binary=$6
    check_prerequisites
    if [[ -z "$mac_binary" ]]; then mac_binary=$(find_mac_binary || true); fi

    if [[ ! -f "$mac_binary" || ! -f "$stabs_binary" ]]; then
        printf 'Source builds currently require two licensed inputs for typed-data generation.\nUse --mac-binary "/path/to/Call of Duty 2 Multiplayer" and --stabs-binary "/path/to/cod2mp_mac_1.3_i386". The binary stays outside the repository and app.\nThe prebuilt release needs only your game data.\n' >&2
        return 1
    fi
    "$root/scripts/build-sdl.sh" "$build/deps"
    local prefix="$build/deps/install" sdk
    sdk=$(/usr/bin/xcrun --show-sdk-path)
    /usr/sbin/taskpolicy -b /usr/bin/nice -n 19 cmake -S "$root" -B "$build/client" \
        -DCOD2_X64=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 "-DCMAKE_OSX_SYSROOT=$sdk" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_MACOS_RELEASE=ON "-DCOD2_MACOS_SDL_PREFIX=$prefix" \
        "-DCMAKE_PREFIX_PATH=$prefix" "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local" \
        "-DZLIB_LIBRARY=$sdk/usr/lib/libz.tbd" "-DZLIB_INCLUDE_DIR=$sdk/usr/include" \
        "-DCOD2_CURL_LIBRARY=$sdk/usr/lib/libcurl.tbd" \
        "-DCOD2_STABS_BINARY=$stabs_binary" "-DCOD2_VALUES_BINARY=$mac_binary" \
        "-DCOD2_TYPED_DATA_DIR=$build/typed" "-DPython3_EXECUTABLE=$python"
    /usr/sbin/taskpolicy -b /usr/bin/nice -n 19 cmake --build "$build/client" --target cod2_macos --parallel 4
    /usr/sbin/taskpolicy -b /usr/bin/nice -n 19 "$python" "$root/tools/cod2x/make_macos_app.py" \
        "$build/client/cod2_macos" "$app" --replace --frameworks "$prefix" --version "$version"
}
