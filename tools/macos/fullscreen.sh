#!/usr/bin/env bash
# Launch the local arm64 client at 333, with no writes to licensed game data.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
binary=${COD2_BINARY:-"$repo/build-macos/cod2_macos"}
data=${COD2_DATA_DIR:-"$HOME/Games/CoD2"}
resolution=${COD2_RESOLUTION:-1920x1080}
borderless=${COD2_BORDERLESS:-1}
run_dir=${COD2_RUN_DIR:-"$repo/output/fullscreen"}
[[ -x $binary && -d $data/main ]] || { echo 'Build cod2_macos and set COD2_DATA_DIR to the licensed game directory.' >&2; exit 2; }
binary=$(cd "$(dirname "$binary")" && pwd)/$(basename "$binary")
case "$resolution" in
    1920x1080|2560x1440|3008x1692|3840x2160|5120x2880|6016x3384) ;;
    *) echo 'COD2_RESOLUTION must be one of the measured 1080p through 6K resolutions.' >&2; exit 2 ;;
esac
[[ $borderless == 0 || $borderless == 1 ]] || { echo 'COD2_BORDERLESS must be 0 or 1.' >&2; exit 2; }
case "$data$run_dir" in
    *\"*|*\;*|*+*|*$'\n'*|*$'\r'*) echo 'Paths cannot contain quotes, semicolons, + or newlines.' >&2; exit 2 ;;
esac
mkdir -p "$run_dir/home"
data=$(cd "$data" && pwd)
run_dir=$(cd "$run_dir" && pwd)
cd "$run_dir"
exec "$binary" +set fs_basepath "\"$data\"" +set fs_homepath "\"$run_dir/home\"" \
    +set r_mode "$resolution" +set r_fullscreen 1 +set r_borderless "$borderless" \
    +set com_maxfps 333 +set r_swapInterval 0 +set in_rawmouse 1 \
    +set m_filter 0 +set cl_mouseAccel 0 +set logfile 0 +set developer 0 \
    +set com_introPlayed 1 "$@"
