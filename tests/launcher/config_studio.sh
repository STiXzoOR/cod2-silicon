#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-studio.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
taskpolicy -b nice -n 19 bash "$root/scripts/build-launcher.sh" "$scratch/core" --test
sdk=$(/usr/bin/xcrun --show-sdk-path)
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos26 -sdk "$sdk" -O -whole-module-optimization -import-objc-header "$root/launcher/NativeSetup.h" \
    "$root/launcher/Core.swift" "$root/launcher/Presentation.swift" \
    "$root/launcher/MatchSummaries.swift" "$root/launcher/ConfigDocument.swift" "$root/launcher/ConfigProfiles.swift" "$root/launcher/ConfigHistory.swift" \
    "$root/launcher/ConfigStudioSession.swift" "$root/launcher/ConfigCatalog.swift" "$root/launcher/ConfigStudioModel.swift" \
    "$root/launcher/InputDevices.swift" "$root/launcher/ConfigBindings.swift" "$root/launcher/ConfigChains.swift" \
    "$root/launcher/ConfigImport.swift" "$root/launcher/ConfigImportSafety.swift" "$root/launcher/ConfigImportCatalogAdapter.swift" \
    "$root/launcher/ConfigRules.swift" "$root/launcher/ConfigCheckup.swift" "$root/launcher/Theme.swift" "$root/launcher/ConfigStudioStyle.swift" \
    "$root/launcher/ConfigStudioBinds.swift" "$root/launcher/ConfigStudioCheckup.swift" \
    "$root/tests/launcher/ConfigStudioModelTests.swift" "$root/tests/launcher/ConfigStudioTabsTests.swift" \
    "$root/tests/launcher/ConfigStudioAcceptanceTests.swift" \
    "$scratch/core/NativeSetup.o" "$scratch/core/NativeShaders.o" "$scratch/core/URL.o" \
    -framework AppKit -framework SwiftUI -lz -o "$scratch/ConfigStudioModelTests"
# Check-up finds its rules beside the executable, as in development builds.
cp "$root/launcher/Resources/ConfigRulesets.json" "$root/launcher/Resources/ConfigOptimizer.json" "$scratch/"
TMPDIR="$scratch" taskpolicy -b nice -n 19 "$scratch/ConfigStudioModelTests" "$root/launcher/Resources/ConfigCatalog.json"
