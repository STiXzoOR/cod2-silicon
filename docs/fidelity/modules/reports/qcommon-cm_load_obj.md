# qcommon/cm_load_obj module reconciliation

Updated 2026-11-04.

**31/31 reconciled: 10 equal, 19 fixed, 2 intentional.** Mac 1.3 is the authority. The full loader at 0x78638-0x79f18, plane loader, recursive partitioner and leaf partition wrapper were read by hand. The Linux helper rows were compared with their corresponding inlined Mac blocks.

## Corrections

**Brush-side bookkeeping included the axial sides.** Mac 0x787e3-0x78807 subtracts six sides per brush before allocating and publishing the general side array. Native kept the total side count. The corrected count also restores the null array when no general sides remain.

**Malformed lump handling differed.** Mac uses unsigned division and logical shifts for lump lengths. Native used signed arithmetic. The native path now matches those operations, along with the exact Mac error messages and localization bytes. Two extra native plane-index checks were removed: Mac indexes the plane array directly at 0x789a3 and 0x79332. The submodel limit check now occurs after allocation and publication, as at 0x78d37-0x78d5a. An extra `TempMalloc(0)` before the temporary node-base query at 0x78d78 was removed.

**A few floating-point selection details differed.** Submodel radius extent selection now uses the Mac absolute-value and subtraction/select order at 0x78f23-0x78f45. Recursive partitioning now preserves Mac unordered straddler acceptance at 0x78390-0x783a0 and the ordered-only back-tolerance update at 0x78344-0x78369. These details principally affect nonfinite input; the established finite partition scoring and recursion already matched.

The existing native box-hull mask, expanded pointer allocations, brush partition corrections and vertex/edge/triangle/border/partition conversions were checked and retained.

## Intentional omissions

The Linux-only `CM_LoadLumpFromFile` and unused `CM_FreeLumpData` utilities have no Mac symbol, implementation or calls in the complete two Mac module bodies. Native has no callers or utility state. Loading uses `Com_GetBsp`, and saving owns its file buffer. No Linux-only ancillary disk reader was added. Linux `CM_GetPlane` corresponds to the existing Mac/native `CM_GetPlaneNum` implementation.

## Verification and limits

The ARM64 Release `cod2_macos_ded` target built with `COD2_X64=ON`, at low priority with two jobs. A before/after preprocessing comparison with `COD2_X64` disabled was byte-identical for both assigned translation units. Final string triage has no mismatches; constant checks and the hand comparison covered allocations, disk strides, masks and float operation order.

No new per-fix tests were written or fixtures changed. No CI or replay was run by this worker. The corrections are compiled and reviewed, but not exercised. Nothing remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-loading.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
