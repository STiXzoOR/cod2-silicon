#!/usr/bin/env bash
# Usage: run.sh GAME_DIRECTORY [engine arguments...]
set -euo pipefail
if [[ $# -lt 1 ]]; then
    echo 'Usage: run.sh GAME_DIRECTORY [engine arguments...]' >&2
    exit 2
fi
repo=$(cd "$(dirname "$0")/../.." && pwd)
binary=${COD2_BINARY:-"$repo/output/build-macos/cod2_macos"}
data=$(cd "$1" && pwd)
shift
[[ -x $binary ]] || { echo "Missing executable: $binary" >&2; exit 2; }
compgen -G "$data/main/*.iwd" >/dev/null || { echo 'Expected GAME_DIRECTORY/main/*.iwd' >&2; exit 2; }
run_dir=${COD2_RUN_DIR:-"$repo/output/macos-run-$(date -u +%Y%m%dT%H%M%SZ)-$$"}
mkdir -p "$run_dir/home"
run_dir=$(cd "$run_dir" && pwd)
# The legacy main concatenates argv without quoting. Preserve paths through
# that second parse by putting literal double quotes in the engine arguments.
case "$data$run_dir" in
    *\"*|*\;*|*+*|*$'\n'*|*$'\r'*) echo 'Paths cannot contain quotes, semicolons, + or newlines' >&2; exit 2 ;;
esac
args=(+set fs_basepath "\"$data\"" +set fs_homepath "\"$run_dir/home\""
      +set logfile 2 +set com_maxfps 250 +set r_swapInterval 0 +set m_filter 0 +set cl_mouseAccel 0)
{
    date -u
    uname -a
    printf 'binary=%s\ndata=%s\n' "$binary" "$data"
    printf 'argv:'
    printf ' %q' "${args[@]}" "$@"
    printf '\n'
} > "$run_dir/launch.txt"
printf 'Logs and writable home: %s\n' "$run_dir"
# Caller arguments come last so settings can be overridden explicitly.
"$binary" "${args[@]}" "$@" 2>&1 | tee "$run_dir/console.log"
