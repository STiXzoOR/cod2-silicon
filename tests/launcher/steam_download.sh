#!/bin/sh
# Setup › Game data › From Steam and Setup's crash reports question, with the launcher's real model.
# The download tool is a stand-in script: Steam and GitHub are never contacted. The model runs in a
# private home folder made here, so no player's settings, key or game data are read or written.
#
#   steam_download.sh --live   instead fetches the real tool and runs it as far as Steam's sign-in
#                              code (GitHub and Steam are contacted; nobody signs in; nothing is
#                              downloaded from Steam). Run it by hand before a release.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-steam.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
taskpolicy -b nice -n 19 bash "$root/scripts/build-launcher.sh" "$scratch/core" --test
sources=""
for file in "$root"/launcher/*.swift; do
    case ${file##*/} in Main.swift) ;; *) sources="$sources $file" ;; esac
done
# shellcheck disable=SC2086
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors -DCOD2_LIQUID_GLASS \
    -target arm64-apple-macos26 -sdk "$(/usr/bin/xcrun --sdk macosx --show-sdk-path)" -O -parse-as-library \
    -import-objc-header "$root/launcher/NativeSetup.h" \
    $sources "$root/tests/launcher/SteamDownloadTests.swift" \
    "$scratch/core/NativeSetup.o" "$scratch/core/NativeShaders.o" "$scratch/core/URL.o" \
    -framework AppKit -framework SwiftUI -framework Network -lz -o "$scratch/core/SteamDownloadTests"
mkdir "$scratch/run" "$scratch/run/home"
HOME="$scratch/run/home" CFFIXED_USER_HOME="$scratch/run/home" "$scratch/core/SteamDownloadTests" "$scratch/run" ${1:+"$1"}
