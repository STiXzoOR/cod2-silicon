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
sources=("$root/launcher/Core.swift" "$root/launcher/Presentation.swift")
views=()
for file in Network Model MapArtwork Theme Artwork RootView HomeView ServersView SettingsView SetupView LibraryView; do
    views+=("$root/launcher/$file.swift")
done
name=CoD2Launcher
if [[ ${2:-} == --test ]]; then
    sources+=("$root/tests/launcher/CoreTests.swift")
    name=LauncherTests
elif [[ ${2:-} == --snapshots ]]; then
    # Review harness: every launcher view with a test-only window that can capture real glass.
    # Like an app bundle, it names its accent colour in an embedded Info.plist and finds the
    # compiled Assets.car beside the executable.
    sources+=("${views[@]}" "$root/tests/launcher/Snapshots.swift")
    name=LauncherSnapshots
    /usr/bin/plutil -create xml1 "$out/Info.plist"
    /usr/bin/plutil -insert CFBundleIdentifier -string io.github.stixzoor.cod2silicon.snapshots "$out/Info.plist"
    if "$root/scripts/compile-launcher-icon.sh" "$out/assets" >/dev/null; then
        cp "$out/assets/Assets.car" "$out/Assets.car"
        /usr/bin/plutil -insert NSAccentColorName -string AccentColor "$out/Info.plist"
    else
        printf 'Rendering with the system accent: this toolchain cannot compile the asset catalog.\n' >&2
    fi
    swift_flags+=(-Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist -Xlinker "$out/Info.plist")
elif [[ ${2:-} == --network-test ]]; then
    sources+=("$root/launcher/Network.swift" "$root/tests/launcher/NetworkTests.swift")
    name=LauncherNetworkTests
else
    sources+=("${views[@]}" "$root/launcher/Main.swift")
fi
/usr/bin/xcrun swiftc -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$sdk" -O -import-objc-header "$root/launcher/NativeSetup.h" \
    "${swift_flags[@]}" "${sources[@]}" "$out/NativeSetup.o" "$out/NativeShaders.o" "$out/URL.o" \
    -framework AppKit -framework SwiftUI -framework Network -lz -o "$out/$name"
# Development builds find the bundled OFL fonts beside the executable; app bundles use Resources/Fonts.
mkdir -p "$out/Fonts"
cp "$root"/launcher/Resources/Fonts/* "$out/Fonts/"
