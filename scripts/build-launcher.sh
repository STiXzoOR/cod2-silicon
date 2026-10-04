#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd -P)
out=${1:?usage: build-launcher.sh OUTPUT [--test]}
mkdir -p "$out"
sdk=$(/usr/bin/xcrun --show-sdk-path)
sdk_version=$(/usr/bin/xcrun --show-sdk-version)
swift_flags=()
if [[ ${sdk_version%%.*} -ge 26 ]]; then swift_flags+=(-DCOD2_LIQUID_GLASS); fi
clang=(/usr/bin/xcrun clang -target arm64-apple-macos13 -isysroot "$sdk" -fobjc-arc -DCOD2_X64=1 -DCOD2_CODX=1)
"${clang[@]}" -c "$root/launcher/NativeSetup.m" -o "$out/NativeSetup.o"
"${clang[@]}" -c "$root/src/platform/cod2x_native_shaders.m" -o "$out/NativeShaders.o"
/usr/bin/xcrun clang -target arm64-apple-macos13 -isysroot "$sdk" -DCOD2_X64=1 -DCOD2_CODX=1 -c "$root/src/PC/qcommon/cod2x_url.c" -o "$out/URL.o"
sources=("$root/launcher/Core.swift")
name=CoD2Launcher
if [[ ${2:-} == --test ]]; then
    sources+=("$root/tests/launcher/CoreTests.swift")
    name=LauncherTests
elif [[ ${2:-} == --network-test ]]; then
    sources+=("$root/launcher/Network.swift" "$root/tests/launcher/NetworkTests.swift")
    name=LauncherNetworkTests
else
    sources+=("$root/launcher/Network.swift" "$root/launcher/Model.swift" "$root/launcher/Design.swift" "$root/launcher/Views.swift" "$root/launcher/Main.swift")
fi
/usr/bin/xcrun swiftc -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$sdk" -O -import-objc-header "$root/launcher/NativeSetup.h" \
    "${swift_flags[@]}" "${sources[@]}" "$out/NativeSetup.o" "$out/NativeShaders.o" "$out/URL.o" \
    -framework AppKit -framework SwiftUI -framework Network -o "$out/$name"
