# WS67 — Original browser map display ordering

2026-10-07, base `12f78f3`, branch `codex/browser-map-order`.

The native server browser now compares the display names returned by the existing
`UI_GetMapDisplayName` helper in map column 3. Previously it compared internal
map IDs, which can reverse the order the player sees. Existing ping tie-breaking,
sort direction and other columns retain their behavior. Both stock and CoD2x use
the correction; the inactive original source remains behind the existing native gate.

## Independent reference

The owned Mac 1.3 binary's `LAN_CompareServers` at `0x181c36` calls
`UI_GetMapDisplayName` at `0x181e47` and `0x181e54` before `I_stricmp` in the map
case. Those calls were inspected in private disassembly. The existing native UI
helper performs case-insensitive map-load-name lookup and returns an unknown ID
unchanged. No proprietary disassembly or assets are included here.

## Verification

`tests/fixes13/browser_maps.c` executes the actual map-name helper and actual
server comparator. It covers local, global and favorite sources; both directions;
internal IDs whose display ordering is reversed; equal displayed names and the
existing direction-independent ping tie; case-insensitive lookup; unknown names;
invalid indices; and unchanged hostname-column sorting.

The final fixture reports nine failures on production revision `12f78f3`, then
zero failures on corrected stock and CoD2x with assertions and AddressSanitizer.
The actual complete `cl_ui_mp.c` and `ui_main_mp.c` Release translation units
compile in both modes. The fixes13 legacy gate reports one changed production
file, ten inactive configurations, zero mismatches. `git diff --check` passes.
Evidence is private under `~/Library/Application Support/CoD2-native-ws63/validation.noindex/browser-*`.

Commands: `python3 tests/fixes13/run.py --build <mode-build> --test browser_maps`
and `python3 tests/fixes13/legacy.py --base 12f78f3`. The baseline adds
`--baseline 12f78f3` to the fixture command. This proves comparator behavior on
synthetic server/map lists; final GUI interaction and full integration gates remain
part of the composed acceptance run. No network service or game was run for this unit.
