#!/bin/bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: play-cod2x.sh [options] [-- game arguments]
  --renderer gl|vulkan|d9vk|dxvk  Default: gl; WineD3D or legacy/modern DXVK
  --backend highball|wine11      Default: highball
  --resolution WIDTHxHEIGHT     Default: 2560x1440
  --fullscreen                  Fullscreen (black screenshot on tested runtime)
  --windowed                    Windowed (default; borderless at desktop size)
  --fps NUMBER                  Default: 333; 0 means uncapped
  --dx9                         Use the game's DirectX 9 code path
  --dx7                         DirectX 7 fallback (default; fastest tested path)
  --local                       Start an empty mp_toujane listen server
  --reference                   Use the diagnostic 1.3 reference executable
  --steam                       Open Windows Steam for account sign-in
  --sync msync|none              Default: none; Steam uses none
  --hud                         Enable the Metal HUD and its logging
  --dry-run                     Show settings and command without changing files
  --help

The supplied Steam executable needs Steam signed into its owning account.
--reference is a separate diagnostic; see WS8-wine-baseline.md for provenance.
Game data must already be copied into the prefix; this script never installs it.
Overrides: COD2_WINE_ROOT, COD2_WINE_ENGINE, COD2_WINE_PREFIX, COD2_WINE11_ROOT,
COD2_WINE_D3D9 (an alternative native DXVK d3d9.dll), COD2_DXVK_HUD.
USAGE
}

fail() { printf 'CoD2x: %s\n' "$*" >&2; exit 1; }
renderer=gl
backend=highball
resolution=2560x1440
fullscreen=0
fps=333
dx7=1
local_map=0
reference=0
steam=0
sync=none
hud=0
dry_run=0
while (($#)); do
    case "$1" in
        --renderer|--backend|--resolution|--fps|--sync)
            (($# >= 2)) || fail "Missing value for $1"
            case "$1" in
                --renderer) renderer=$2 ;;
                --backend) backend=$2 ;;
                --resolution) resolution=$2 ;;
                --fps) fps=$2 ;;
                --sync) sync=$2 ;;
            esac
            shift 2 ;;
        --fullscreen) fullscreen=1; shift ;;
        --windowed) fullscreen=0; shift ;;
        --dx7) dx7=1; shift ;;
        --dx9) dx7=0; shift ;;
        --local) local_map=1; shift ;;
        --reference) reference=1; shift ;;
        --steam) steam=1; shift ;;
        --hud) hud=1; shift ;;
        --dry-run) dry_run=1; shift ;;
        --help|-h) usage; exit 0 ;;
        --) shift; break ;;
        *) fail "Unknown option: $1 (use -- before game arguments)" ;;
    esac
done
case "$renderer" in gl|vulkan|d9vk|dxvk) ;; *) fail "Unknown renderer: $renderer" ;; esac
case "$backend" in highball|wine11) ;; *) fail "Unknown backend: $backend" ;; esac
case "$sync" in msync|none) ;; *) fail "Unknown sync mode: $sync" ;; esac
[[ "$resolution" =~ ^[1-9][0-9]{2,4}x[1-9][0-9]{2,4}$ ]] || fail "Invalid resolution: $resolution"
[[ "$fps" =~ ^[0-9]{1,4}$ ]] || fail "FPS must be 0..1000"
((10#$fps <= 1000)) || fail "FPS must be 0..1000"

root=${COD2_WINE_ROOT:-"$HOME/Library/Application Support/CoD2x-Wine"}
engine=${COD2_WINE_ENGINE:-"$root/highball/engines/x64-sikarugir10.0_6-r19"}
if [[ "$backend" == highball ]]; then
    export WINEPREFIX=${COD2_WINE_PREFIX:-"$root/highball/bottles/cod2x"}
    wine="$engine/engine/bin/wine"
    frameworks="$engine/frameworks"
    export DYLD_FALLBACK_LIBRARY_PATH="$frameworks:$frameworks/GStreamer.framework/Versions/1.0/lib"
    export DYLD_FALLBACK_FRAMEWORK_PATH="$frameworks"
    export GST_PLUGIN_PATH="$frameworks/GStreamer.framework/Versions/1.0/lib/gstreamer-1.0"
    if [[ -f "$frameworks/libhbaudiobuf.dylib" ]]; then
        export DYLD_INSERT_LIBRARIES="$frameworks/libhbaudiobuf.dylib"
    fi
else
    export WINEPREFIX=${COD2_WINE_PREFIX:-"$root/gcenx-prefix"}
    wine11=${COD2_WINE11_ROOT:-"$root/downloads/Wine Devel.app/Contents/Resources/wine"}
    wine="$wine11/bin/wine"
    [[ "$renderer" != gl ]] || fail "Gcenx Wine 11.18 was built without OpenGL; use vulkan or dxvk"
fi
[[ -x "$wine" ]] || fail "Wine is missing: $wine (setup commands are in the WS8 report)"
[[ -f "$WINEPREFIX/drive_c/windows/syswow64/kernel32.dll" ]] || fail "Prefix has no Windows 32-bit support: $WINEPREFIX"
game="$WINEPREFIX/drive_c/Games/CoD2"
exe="$game/CoD2MP_s.exe"
if ((reference)); then exe="$game/CoD2MP_reference_s.exe"; fi
export WINEDEBUG=${WINEDEBUG:-fixme-all}
export WINEESYNC=0
export WINEMSYNC=0
if [[ "$sync" == msync && "$backend" == highball ]] && ((!steam)); then export WINEMSYNC=1; fi
export WINEDLLOVERRIDES='winemenubuilder.exe=d;mss32=n,b'
export CX_FWD_COMPAT_GL_CTX=1
export DISABLE_LSFGM=1
export MTL_HUD_ENABLED=$hud
export MTL_HUD_LOG_ENABLED=$hud
unset WINEDLLPATH_PREPEND DXVK_FRAME_RATE DXVK_CONFIG_FILE

args=()
if ((steam)); then
    exe="$WINEPREFIX/drive_c/Program Files (x86)/Steam/steam.exe"
else
    [[ -f "$game/mss32.dll" && -f "$game/mss32_original.dll" ]] || fail "Install both CoD2x release DLLs in $game"
    args=(+set logfile 2 +set com_introPlayed 1 +set com_freezeWatch 0
          +set developer 0 +set r_fullscreen "$fullscreen" +set r_mode "$resolution"
          +set r_vsync 0 +set r_swapInterval 0 +set cg_drawFPS 1
          +set com_maxfps "$fps" +set m_rinput 2)
    if ((dx7)); then args+=(+set r_rendererPreference dx7); else args+=(+set r_rendererPreference dx9); fi
    args+=(+exec cod2x-wine.cfg)
    if ((local_map)); then args+=(+set dedicated 0 +set g_gametype dm); fi
fi
[[ -f "$exe" ]] || fail "Executable is missing: $exe"
printf 'Backend: %s; renderer: %s; %s; fullscreen=%s; maxfps=%s; sync=%s\n' \
    "$backend" "$renderer" "$resolution" "$fullscreen" "$fps" "$WINEMSYNC"
printf 'Prefix: %s\nExecutable: %s\nConsole log: %s/main/console_mp.log\n' "$WINEPREFIX" "$exe" "$game"
if ((dry_run)); then
    printf 'Command:'; printf ' %q' "$wine" "$exe" "${args[@]}" "$@"; printf '\n'
    exit 0
fi

if ((!steam)); then
    # CoD2x registers its watchdog after the game's startup dvars, resetting it.
    # Apply this again once initialization has returned to the command buffer.
    {
        printf 'wait 60\nset com_freezeWatch 0\nset developer 0\n'
        if ((local_map)); then
            printf 'set sv_cheats 1\ndevmap mp_toujane\nwait 60\nset developer 0\n'
        fi
    } > "$game/main/cod2x-wine.cfg"
    case "$renderer" in
        gl|vulkan)
            export WINEDLLOVERRIDES="$WINEDLLOVERRIDES;d3d9=b"
            "$wine" reg add 'HKCU\Software\Wine\Direct3D' /v renderer /t REG_SZ /d "$renderer" /f >/dev/null ;;
        d9vk|dxvk)
            if [[ "$renderer" == d9vk ]]; then
                dll="$engine/frameworks/renderer/d9vk/wine/i386-windows/d3d9.dll"
            else
                dll="$engine/renderers/d9vk-modern/wine/i386-windows/d3d9.dll"
            fi
            dll=${COD2_WINE_D3D9:-"$dll"}
            [[ -f "$dll" ]] || fail "Renderer DLL is missing: $dll"
            cp "$dll" "$game/d3d9.dll"
            # Highball's asNative copy step removes this marker from DXVK's DOS stub.
            # Leave the pinned engine DLL and every game executable untouched.
            marker=$(dd if="$game/d3d9.dll" bs=1 skip=64 count=16 2>/dev/null)
            if [[ "$marker" == 'Wine builtin DLL' ]]; then
                dd if=/dev/zero of="$game/d3d9.dll" bs=1 seek=64 count=17 conv=notrunc 2>/dev/null
            fi
            export WINEDLLOVERRIDES="$WINEDLLOVERRIDES;d3d9=n"
            export DXVK_LOG_PATH="$game"
            # DXVK 3.1's text HUD uses DrawIndex, unsupported by this MoltenVK.
            # Use the game's cg_drawFPS and the independent Metal HUD instead.
            export DXVK_HUD=${COD2_DXVK_HUD:-}
            ;;
    esac
fi
cd "$(dirname "$exe")"
exec "$wine" "$exe" "${args[@]}" "$@"
