#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd -P)
# shellcheck source=scripts/macos-common.sh
source "$root/scripts/macos-common.sh"
version=$(< "$root/RELEASE_VERSION")
build="$root/build/package/release"
mac_binary=${COD2_VALUES_BINARY:-}
stabs_binary=${COD2_STABS_BINARY:-}
while [[ $# -gt 0 ]]; do
    case $1 in
        --version) version=${2:?missing version}; shift 2 ;;
        --build-dir) build=${2:?missing directory}; shift 2 ;;
        --mac-binary) mac_binary=${2:?missing path}; shift 2 ;;
        --stabs-binary) stabs_binary=${2:?missing path}; shift 2 ;;
        *) printf 'Usage: %s [--version N.N.N] [--build-dir DIR] [--mac-binary FILE --stabs-binary FILE]\n(version defaults to RELEASE_VERSION; the two reference binaries are optional and only verify the committed typed-data snapshot)\n' "$0" >&2; exit 2 ;;
    esac
done
if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then printf 'Version must be N.N.N.\n' >&2; exit 2; fi
# Signing: COD2_SIGN_IDENTITY names a codesigning identity (default "-", ad hoc).
# Notarization: COD2_NOTARY_PROFILE names a notarytool keychain profile, made with
# `xcrun notarytool store-credentials`; it needs a Developer ID Application identity.
identity=${COD2_SIGN_IDENTITY:--}
profile=${COD2_NOTARY_PROFILE:-}
if [[ -n $profile && $identity != "Developer ID Application:"* ]]; then
    printf 'Notarization needs COD2_SIGN_IDENTITY set to a "Developer ID Application: ..." identity.\n' >&2
    exit 2
fi
mkdir -p "$build" "$root/dist"
build=$(cd "$build" && pwd -P)
app="$build/CoD2 Silicon.app"
COD2_SIGN_IDENTITY=$identity build_app "$root" "$build" "$app" "$version" "$mac_binary" "$stabs_binary"
signing="ad-hoc signed, not notarized"
[[ $identity == "-" ]] || signing="signed by $identity, not notarized"
if [[ -n $profile ]]; then
    submission="$build/notarize-$version.zip"
    /usr/bin/ditto -c -k --keepParent "$app" "$submission"
    /usr/bin/xcrun notarytool submit "$submission" --keychain-profile "$profile" --wait
    rm -f "$submission"
    /usr/bin/xcrun stapler staple "$app"
    /usr/bin/xcrun stapler validate "$app"
    /usr/sbin/spctl --assess --type execute --verbose=2 "$app"
    signing="signed by $identity, notarized and stapled"
fi
archive="CoD2-Silicon-$version-macos-arm64.zip"
/usr/bin/ditto -c -k --keepParent "$app" "$root/dist/$archive"
(cd "$root/dist" && /usr/bin/shasum -a 256 "$archive" > SHA256SUMS)
printf 'Created dist/%s and dist/SHA256SUMS (%s).\n' "$archive" "$signing"
