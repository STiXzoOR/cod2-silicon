# Native display modes

The in-game `r_mode` enum is rebuilt at renderer startup and `vid_restart`.
`MacPlatform_SelectMonitor` takes the selected display's native pixel dimensions
from a CoreGraphics mode carrying `kDisplayModeNativeFlag`. The current Retina
backing size is not a panel limit: a 5120 × 2880 panel can have a 6016 × 3384
backing when the desktop is scaled. If no native mode is advertised (including
some virtual displays), use SDL's desktop dimensions conservatively and log the
fallback.

The native link must use CoreGraphics' `CGGetActiveDisplayList` and
`CGDisplayBounds`, not the legacy placeholders in `link_stubs.c`.

`MacPlatform_BuildModeNames` is a pure function in `macos_display_modes.h`.
It combines the native size, SDL's modes, and these standard render sizes:
640×480, 800×600, 1024×768, 1280×720, 1920×1080, 2560×1440, 3008×1692,
3840×2160, 5120×2880, 6016×3384. Keep sizes at least 640×480 that fit in **both**
native dimensions. Deduplicate width/height pairs and sort by width, then height.
Refresh-rate duplicates do not become separate resolutions. The owned Windows
1.3 renderer confirms this ordering at `0x10012380` and its minimum/duplicate
filter at `0x10012510`. Native dimensions and standard sizes compensate for SDL
reporting Retina modes in points.

With no extra SDL sizes, the resulting lists are:

| Native panel | Standard sizes retained, followed by native if different |
| --- | --- |
| 5120×2880 | 640×480, 800×600, 1024×768, 1280×720, 1920×1080, 2560×1440, 3008×1692, 3840×2160, 5120×2880 |
| 6016×3384 | All of the above, then 6016×3384 |
| 1920×1080 | 640×480, 800×600, 1024×768, 1280×720, 1920×1080 |
| Notebook example: 3024×1964 | 640×480, 800×600, 1024×768, 1280×720, 1920×1080, 2560×1440, 3008×1692, 3024×1964 |

A notebook's actual native size and any fitting driver sizes (for example,
1512×982) are inserted at their sorted position. There is no fixed notebook list.

`R_RegisterDvars` copies the pending size **by name** before rebuilding the
static enum storage, then updates the domain. An absent saved size selects the
largest offered size that fits inside the requested width and height; malformed
values use the first mode. Valid numeric enum indices still work. Thus a saved
6016×3384 request becomes 5120×2880 on a 5K panel, or 1920×1080 on today's virtual
1080p display. It cannot silently retain a 6K render target.
`Dvar_SetFromStringFromSource` applies the same rule when a startup `+set` is
replayed after registration or an old configuration is imported later. This
keeps the valid fallback saved instead of latching the generic enum reset.

Windows uses `r_monitor 0` for the default/window monitor, `1` for the first
monitor, `2` for the second, and so on (`0x100127d0`). Native enumeration and
fullscreen placement now honor that selection. An unavailable index falls back
to the first display. A display change requires `vid_restart` or relaunch to
refresh the enum and backing; there is no automatic hot-plug resize in the middle
of a frame. As before, a listen server rejects `vid_restart`; disconnect first.
Physical multi-monitor and Retina acceptance needs the corresponding hardware.

The launcher should implement the same input/filter/deduplication/sort rule,
selecting its intended screen and using native pixels rather than a scaled
backing. It should normalize a saved size before launch by the same rule and
refresh when the display selection or topology changes. `GameSettings.resolutions`
is a separate fixed list and is outside this engine change.
