#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
models=${1:?Supply the reviewed engine repository root}
output=${2:?Supply a task-private output directory}
mkdir -p "$output"
sdk=$(/usr/bin/xcrun --show-sdk-path)
architecture=$(uname -m)
case "$architecture" in arm64|x86_64) ;; *) exit 1 ;; esac
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target "$architecture-apple-macos13" -sdk "$sdk" -O \
    "$models/launcher/ConfigDocument.swift" "$models/launcher/ConfigImport.swift" \
    "$models/launcher/ConfigImportSafety.swift" "$models/launcher/ConfigCatalog.swift" \
    "$models/launcher/ConfigImportCatalogAdapter.swift" "$here/Audit.swift" -o "$output/audit"
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target "$architecture-apple-macos13" -sdk "$sdk" -O -parse-as-library "$here/Sign.swift" -o "$output/sign"
cp "$models/launcher/Resources/ConfigCatalog.json" "$output/ConfigCatalog.json"
