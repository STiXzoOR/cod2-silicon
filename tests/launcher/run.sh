#!/bin/sh
set -eu
root=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
out="$root/output/ws28/unit"
mkdir -p "$out"
"$root/scripts/build-launcher.sh" "$out" --test
"$out/LauncherTests" "$root/tests/launcher/fixtures"

python3 "$root/tests/launcher/network.py"
sh "$root/tests/launcher/artwork.sh"
"$root/scripts/fetch-launcher-fonts.sh" --check

# Every view must also compile for SDKs before macOS 26, where only the material styling exists.
sdk=$(/usr/bin/xcrun --show-sdk-path)
views=""
for file in Core Presentation Network Model MapArtwork Theme Artwork RootView HomeView ServersView SettingsView SetupView LibraryView Main; do
    views="$views $root/launcher/$file.swift"
done
# shellcheck disable=SC2086
/usr/bin/xcrun swiftc -typecheck -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$sdk" -import-objc-header "$root/launcher/NativeSetup.h" $views
echo "PASS: views typecheck without COD2_LIQUID_GLASS (macOS 13-25 material branch)"

# Render every screen in both appearances through the review harness and check the images.
python3 "$root/tests/launcher/snapshots.py" "$out/snapshots"
