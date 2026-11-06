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

## Existing fixture preflight

Before integration, the existing timeout and map-loading fixtures failed to compile because the restored authorization/disconnect paths referenced services their isolated harnesses did not supply. The fixtures now provide those adapters without changing their cases or assertions. Stock and CoD2x ASan/UBSan runs pass. The existing stock scheduling suite also passes all13 groups.

The match-summary lifecycle fixture had a separate missing `CL_ClearStaticDownload` adapter. Running it against the original03ebea4 lifecycle functions reproduced the same link failure. That adapter was added to the existing fixture; stock and CoD2x lifecycle cases now pass. These checks cover their established scenarios, not all restored authorization or detached-download branches. No new tests were added, and production source is unchanged by these fixture repairs.

## Integrated batch verification

Integrated source `3ee60be` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.

## Batch 17 follow-up

CL_SyncGpu and CL_RunOncePerClientFrame are now reconciled by renderer-gpu-sync.md. The API59 void callback owns the original budget snapshot/poll loop through correctly typed bindings and a shared synthetic native cycle clock. Frame clamp arithmetic follows the original unsigned delta. Earlier unresolved timing statements above describe the pre-follow-up source; no runtime wall-clock equivalence is claimed.
