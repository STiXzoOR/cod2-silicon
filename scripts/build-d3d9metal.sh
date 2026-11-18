#!/bin/bash
# Build libd3d9metal, the Direct3D 9 on Metal library the Metal engines link, from a
# pinned, checksum-verified source archive.
#
# The source is mtld3d (zlib licence, github.com/athei/mtld3d), from the fork that
# carries its native host build (github.com/kanapitsas/mtld3d): the library linked into
# an application that is not a Windows program under Wine. The archive comes from this
# project's own copy of that fork (github.com/cod2unbound/mtld3d, branch cod2-unbound).
# The commit below is that fork's 101f37d3, which was compared file by file with a clone
# on 2026-11-17, plus two changes of this project's. A window with vertical sync off is
# given the newest frame when the display has room for one, where every frame used to
# wait its turn and held the game a little above 300 frames a second. And a copy of a
# whole anti-aliased target into a plain one, which is how Direct3D 9 resolves its
# samples, is taken as the resolve itself (render.directResolve, which mac_d3d9.c turns
# on) where the samples used to be resolved and the result copied. The tag
# cod2-unbound-0.4-pin-3 keeps the commit.
#
# mtld3d is written in Rust, so this needs cargo 1.99 or later (`brew install rust`, or
# rustup). cargo fetches the crates the library uses and checks each against the
# checksum in the archive's Cargo.lock (--locked); nothing newer is ever picked up.
set -euo pipefail
cache=${1:?usage: build-d3d9metal.sh build-directory}
mkdir -p "$cache"
cache=$(cd "$cache" && pwd -P)
prefix="$cache/install"
commit=9c28df30afebcde4644feed738cc6f35cbf779a9
archive="mtld3d-$commit.tar.gz"
checksum=643502c9ec40653af5692a7c9cef2ca269e3d485d9e1c815790ab41b891b7306
if [[ $(/usr/bin/uname -s) != Darwin || $(/usr/bin/uname -m) != arm64 ]]; then
    printf 'libd3d9metal is built for Apple silicon Macs only.\n' >&2
    exit 1
fi
if ! command -v cargo >/dev/null 2>&1; then
    printf 'The Metal engines need Rust to build: brew install rust, or https://rustup.rs\n' >&2
    exit 1
fi
cargo_version=$(cargo --version | /usr/bin/awk '{print $2}')
if [[ $(printf '%s\n' 1.99.0 "$cargo_version" | /usr/bin/sort -t. -k1,1n -k2,2n -k3,3n | /usr/bin/head -n 1) != 1.99.0 ]]; then
    printf 'The Metal engines need cargo 1.99 or later; this one is %s.\n' "$cargo_version" >&2
    exit 1
fi
if [[ ! -f "$cache/$archive" ]]; then
    curl --fail --location --retry 3 "https://github.com/cod2unbound/mtld3d/archive/$commit.tar.gz" -o "$cache/$archive.part"
    mv "$cache/$archive.part" "$cache/$archive"
fi
if [[ $(/usr/bin/shasum -a 256 "$cache/$archive" | /usr/bin/awk '{print $1}') != "$checksum" ]]; then
    printf 'Checksum mismatch: %s. Remove it and retry.\n' "$cache/$archive" >&2
    exit 1
fi
stamp="mtld3d-$commit production cargo-$cargo_version macos-26.0 v1"
if [[ -f "$prefix/.complete" && $(cat "$prefix/.complete") == "$stamp" ]]; then
    exit 0
fi
source="$cache/mtld3d-$commit"
rm -rf "$prefix" "$source"
tar -xf "$cache/$archive" -C "$cache"
# Full speed by default; COD2_BUILD_BACKGROUND=1 keeps builds on efficiency cores.
run() {
    if [[ ${COD2_BUILD_BACKGROUND:-0} == 1 ]]; then
        /usr/sbin/taskpolicy -b /usr/bin/nice -n 19 "$@"
    else
        "$@"
    fi
}
# `production` is the profile mtld3d ships: its `release` keeps the debug assertions
# and overflow checks its tests run with.
(cd "$source/native" && MACOSX_DEPLOYMENT_TARGET=26.0 CARGO_TARGET_DIR="$cache/target" \
    run cargo build --profile production --locked >"$cache/cargo-build.log" 2>&1) || {
    /usr/bin/tail -n 30 "$cache/cargo-build.log" >&2
    exit 1
}
mkdir -p "$prefix/lib" "$prefix/include" "$prefix/licenses"
cp "$cache/target/production/libd3d9metal.dylib" "$prefix/lib/"
/usr/bin/install_name_tool -id @rpath/libd3d9metal.dylib "$prefix/lib/libd3d9metal.dylib"
/usr/bin/codesign --force --sign - "$prefix/lib/libd3d9metal.dylib"
cp "$source/native/include/d3d9metal.h" "$prefix/include/"
cp "$source/LICENSE" "$prefix/licenses/mtld3d-LICENSE.txt"
# The crates linked into the library, each with the licence files it ships.
python=$(/usr/bin/xcrun --find python3)
(cd "$source/native" &&
    cargo tree --locked --offline --target aarch64-apple-darwin -e normal --prefix none --format '{p}' >"$cache/crates.txt" &&
    cargo metadata --locked --offline --filter-platform aarch64-apple-darwin --format-version 1 >"$cache/metadata.json")
"$python" - "$cache/crates.txt" "$cache/metadata.json" "$source" "$prefix/licenses/mtld3d-crates.txt" <<'EOF'
import json, pathlib, sys

crates, metadata, source, output = sys.argv[1:]
linked = {tuple(line.split()[:2]) for line in pathlib.Path(crates).read_text().splitlines() if line.strip()}
packages = {(p["name"], "v" + p["version"]): p for p in json.loads(pathlib.Path(metadata).read_text())["packages"]}
parts = ["The Rust crates linked into libd3d9metal.dylib, with the licence files each one ships.\n"]
for name, version in sorted(linked):
    package = packages[(name, version)]
    folder = pathlib.Path(package["manifest_path"]).parent
    if str(folder).startswith(source):
        continue  # mtld3d's own crates: mtld3d-LICENSE.txt
    files = sorted(p for p in folder.iterdir()
                   if p.is_file() and p.name.upper().startswith(("LICENSE", "LICENCE", "COPYING", "NOTICE", "UNLICENSE")))
    licence = package.get("license")
    if not files and not licence:
        sys.exit(f"{name} {version} states no licence")
    parts.append(f"\n{'=' * 78}\n{name} {version} ({licence or 'see below'})\n{'=' * 78}\n")
    if not files:
        parts.append(f"\nThe crate ships no licence file. Its licence is {licence}; its source is {package.get('repository')}.\n")
    for file in files:
        parts.append(f"\n--- {file.name} ---\n{file.read_text(errors='replace').rstrip()}\n")
pathlib.Path(output).write_text("".join(parts))
EOF
printf '%s\n' "$stamp" > "$prefix/.complete"
