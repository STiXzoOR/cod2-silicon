# cgame/cg_effects_load_obj module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 1 equal, 2 fixed.** Compared full Mac bodies and all eleven effect-type strings. Legacy source is unchanged under COD2_X64=OFF.

## Corrections

A malformed CSV continued into later files in its directory. Mac 0x1e7c9d returns after reporting that parse error. Restored that exit.

Mac 0x1e7ded allocates one 65536-byte low-temp file-list buffer and shares it across the global and map directories. Native used separate stack buffers and had an unused caller stack buffer. Restored the shared allocation and argument. Per-file CSV contents still use the high-temp arena and effect-name copies use low-temp memory.

The successful table name is an empty string at Mac 0x1e7f4e; native used a fabricated name. The missing-entry diagnostic had an extra newline, and its path cleared low-temp memory despite Mac returning before the clear at 0x1e7f9f. Both now follow Mac, including that error-path lifetime quirk.

## Comparison examples and storage

Compared file caps, case-insensitive sorting, open/read/close, CSV comments and row errors, all 11x23 registrations, and success/error returns. All eleven names match the Mac table at 0x36e7e0. The comparator and table retain full native pointer widths, and existing FX registration providers remain in place. The client compile retains the existing strlen-to-int warning.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built at low priority with two jobs. It excludes cgame, so all five assigned client translation units were also compiled individually with this worktree's actual `cod2_macos` compile commands. OFF preprocessing matches the base with real diagnostic line expansions preserved. No new tests, full CI or replays were run. Changes are compiled and reviewed, not exercised in a running client.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-world.noindex/`.
