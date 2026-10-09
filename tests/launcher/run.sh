#!/bin/sh
set -eu
root=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
# Keep review captures and temporary builds in task-private directories when requested.
out="${COD2_LAUNCHER_TEST_OUTPUT:-$root/output/ws28/unit}"
mkdir -p "$out"
"$root/scripts/build-launcher.sh" "$out" --test
"$out/LauncherTests" "$root/tests/launcher/fixtures"

python3 "$root/tests/launcher/network.py"
sh "$root/tests/launcher/artwork.sh"
sh "$root/tests/launcher/config_document.sh"
sh "$root/tests/launcher/config_profiles.sh"
sh "$root/tests/launcher/config_launch.sh"
sh "$root/tests/launcher/config_studio.sh"
sh "$root/tests/launcher/config_catalog.sh"
sh "$root/tests/launcher/config_import.sh"
sh "$root/tests/launcher/config_import_session.sh"
sh "$root/tests/launcher/config_directory.sh"
sh "$root/tools/config-directory/test.sh"
sh "$root/tests/launcher/config_rules.sh"
sh "$root/tests/launcher/config_checkup.sh"
sh "$root/tests/launcher/config_bindings.sh"
sh "$root/tests/launcher/config_ready_check.sh"
sh "$root/tests/launcher/input_devices.sh"
sh "$root/tests/launcher/config_chains.sh"
"$root/scripts/fetch-launcher-fonts.sh" --check

# One shape system: buttons take their style from Theme.swift (capsules, or circles for a lone
# symbol) or are plain text and glyph buttons. The review harness checks the rendered geometry.
if grep -n -E '\.buttonStyle\(|\.buttonBorderShape\(' "$root"/launcher/*.swift |
    grep -v -E 'launcher/Theme\.swift:|\.buttonStyle\(\.plain\)|\.buttonStyle\(RowButtonStyle'; then
    echo "FAIL: button styles outside launcher/Theme.swift" >&2
    exit 1
fi
echo "PASS: every button style comes from launcher/Theme.swift"

# Every view must also compile for SDKs before macOS 26, where only the material styling exists.
sdk=$(/usr/bin/xcrun --show-sdk-path)
views=""
for file in Core Presentation ConfigDocument ConfigProfiles ConfigHistory ConfigStudioSession ConfigCatalog ConfigRules ConfigReadyCheck \
    ConfigCheckup ConfigBindings InputDevices ConfigChains ConfigImport ConfigImportSafety WindowsProfiles ConfigImportSession \
    ConfigImportCatalogAdapter ConfigDirectory ConfigStudioModel ConfigStudioStyle \
    Network Model MapArtwork Theme Artwork RootView HomeView ServersView SettingsView SetupView LibraryView ConfigsView ConfigSettingsView ConfigHistoryView Main; do
    views="$views $root/launcher/$file.swift"
done
# shellcheck disable=SC2086
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 -typecheck -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$sdk" -import-objc-header "$root/launcher/NativeSetup.h" $views
echo "PASS: views typecheck without COD2_LIQUID_GLASS (macOS 13-25 material branch)"

# Render every screen in both appearances through the review harness and check the images.
python3 "$root/tests/launcher/snapshots.py" "${COD2_LAUNCHER_SNAPSHOT_OUTPUT:-$out/snapshots}"
