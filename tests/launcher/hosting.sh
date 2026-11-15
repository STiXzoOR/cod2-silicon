#!/bin/sh
# Servers › Hosting: the config writer, zPAM release checks, export and saved list.
# With COD2_HOSTING_LIVE="<game app>|<game data>" it also starts both dedicated servers on
# 127.0.0.1 ports 28991 and 28992; CoD2x's services are pointed at a closed local port for that.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-hosting.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
# The model is built with the launcher's own core, so a server start writes through the real
# session log (GameSessionLog), not a stand-in.
taskpolicy -b nice -n 19 bash "$root/scripts/build-launcher.sh" "$scratch/core" --test
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos26 -sdk "$(/usr/bin/xcrun --sdk macosx --show-sdk-path)" -O -parse-as-library \
    -import-objc-header "$root/launcher/NativeSetup.h" \
    "$root/launcher/Core.swift" "$root/launcher/Presentation.swift" \
    "$root/launcher/Hosting.swift" "$root/launcher/HostingModel.swift" "$root/tests/launcher/HostingTests.swift" \
    "$scratch/core/NativeSetup.o" "$scratch/core/NativeShaders.o" "$scratch/core/URL.o" \
    -framework AppKit -lz -o "$scratch/HostingTests"
mkdir "$scratch/run"
COD2X_TEST_SERVICE_URL=http://127.0.0.1:9 COD2X_TEST_UPDATER_ADDRESS=127.0.0.1:9 \
    COD2X_TEST_UPDATER_URL=http://127.0.0.1:9/api/updater/check "$scratch/HostingTests" "$scratch/run"
