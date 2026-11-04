# client/cl_main_mp module reconciliation

Updated 2026-11-04.

**96 entries reviewed: 68 equal, 12 fixed, 14 intentional, 2 unchecked.** Mac 1.3 i386 is the reference. The native-OFF source is unchanged.

Every assigned body was read against the Mac disassembly. The reliable-command and packet wrappers were followed into their native inline helpers. All 58 stock client dvar registrations match Mac in type, default, range and flags. The seven console-font jump-table targets at 0x33ae20 were decoded directly.

## Corrections

- `CL_NextDemo` was missing. Mac 0x1472ea copies and clears `nextdemo`, queues its command and a newline, then executes the command buffer. `CL_DemoCompleted` now follows Mac 0x14c0c9 by disconnecting and invoking that function, instead of raising the native disconnect error.
- `CL_InitRenderer` now selects the language-specific console fonts used by Mac 0x147cf0. `CL_InitOnceForAllClients` includes the missing `unskippablecinematic` registration from Mac 0x148fbd, using the identical guarded declaration and registration already reviewed with the cinematic module.
- `CL_VoicePacket` now truncates the talker to a byte before validation, as Mac 0x147932 does. The old integer retained a negative read-failure result. `CL_Frame` now passes the current voice queue slot, as Mac 0x14b1a1 does, and restores periodic authorization after 300,000 ms from Mac 0x14b265.
- `CL_ConnectionlessPacket` restores the detached HTTP-download disconnect branch at Mac 0x14ca53 and the full 0x20000-byte print capacity at 0x14cc05.
- `CL_MapLoading` sets the localhost name before disconnecting and initializes the resend clock to -2000, matching Mac 0x14bf76–0x14bf90. `CL_ForwardCommandToServer` no longer silently suppresses recognized CGame command names; Mac 0x14d192 has no such gate.
- `CL_DownloadsComplete` retains the pause set before CGame initialization, matching Mac 0x14d3bc. The native extra unpause was checked against the CGame initialization and dvar-registration lifecycle before removal.
- Skeleton timestamps now increment with defined unsigned wrapping, matching Mac 0x147464. Logo height scaling now converts to float before doubling, matching the SSE order at Mac 0x149d9e.

## Retained native contracts

UDP downloads remain in the native connection structure. `CL_BeginDownload`, `CL_ParseDownload` and `CL_Disconnect` consistently share its names, block/count and file handle; HTTP redirects use the mirrored names in client-static storage. Moving only cleanup to Mac's storage would break that existing chain.

The native renderer enforces API 59. Its import table omits Mac API 60's `Dvar_FindVar` slot and adds the native `CM_GetPlaneNum` callback. The assigned slots were decoded through the Mac GOT; the five unassigned slots are also unassigned in Mac. This reconciliation preserves that complete native ABI contract.

The five removed PunkBuster callbacks have no native callers and remain behind the documented null-adapter boundary. Engine authorization and packet dispatch were checked separately. CommonCrypto replaces the seeded MD5 backend. Native demo scratch arrays and the short-name guard before checking a demo extension remain defined native adaptations. Optional CoD2x behavior and local match-summary hooks remain in place.

## Verification and limits

The changed client object compiles in stock and CoD2x arm64 Release configurations. The arm64 `cod2_macos_ded` target also builds, with low priority and two jobs; that target excludes client sources. Five native-OFF preprocessing configurations match the assigned baseline; this is not an i386 object comparison. No new tests, full CI or replays were run. The corrections are compiled and reviewed, not exercised.

`CL_SyncGpu` and `CL_RunOncePerClientFrame` remain unchecked. Mac 0x14a726 obtains an integer cycle budget, polls a GPU fence with elapsed-tick limits and sleeps between polls. Native `refexport_t.SyncGpu` and `RB_AdaptiveGpuSyncWait` return void, and the renderer's `rdtsc_lo` is a zero-returning stub. Restoring that behavior requires the renderer/header/timer owner; no return-value cast or invented timing contract was introduced. The rest of the frame-timing caller was compared.

The complete 96-entry private ledger is `client-main.ledger.json` in the reconciliation unit directory. Private evidence and the renderer dependency manifest are under `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-main.noindex/`.
