#!/bin/sh
# Run from any directory. The output directory must be local/untracked.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
database=${1:?usage: tools/abi/check.sh compile_commands.json output-directory}
output=${2:?usage: tools/abi/check.sh compile_commands.json output-directory}
binary=${3:-${COD2_STABS_BINARY:-$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386}}
mkdir -p "$output"
python3 "$root/tools/abi/test_audit.py"
python3 "$root/tools/abi/test_imports.py"
python3 "$root/tools/abi/audit.py" "$database" --output "$output/functions.json" \
    --baseline "$root/tools/abi/baseline.json"
python3 "$root/tools/abi/callback_tables.py" "$database" --audit "$output/functions.json" \
    --baseline "$root/tools/abi/callback-baseline.json" --output "$output/callbacks.json"
if [ ! -e "$binary" ]; then
    echo "SKIP retail import ABI check: missing private STABS input: $binary"
    exit 0
fi
python3 "$root/tools/abi/imports.py" --compile-commands "$database" \
    --binary "$binary" --json "$output/imports.json" --check
