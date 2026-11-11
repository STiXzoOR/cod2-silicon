# EffectsCore/FxUtil module reconciliation

Updated 2026-11-05.

134 of 138 rows reconciled: 84 equal, 16 fixed, 34 intentional. Four Linux-only
rows remain unchecked because their Mac identity or callsite could not be
established. Mac 1.3 is the behavioral authority. Linux supplied names only.
No reference source, disassembly or binaries are included in this change.

The 37 assigned Mac entries were read, together with their inline allocation,
accessor, list, sorting and lifecycle contracts. Coalesced visibility and Flash
cull bodies were read in `__TEXT,__textcoal_nt` at 0x2ef95c–0x2ef970. The repeated
static-initializer symbol needs address-bounded output: `otool -p` selects an
unrelated initializer at 0x848a instead of this module's 0x58dc4.

## Corrections

- `FX_CalcOriginAndAxis`, 0x59856: cylinder offsets multiply random height by
  template height before multiplying each axis component; the perpendicular
  axis uses `cross(up, forward)`; sphere angles multiply by the binary64
  radians constant before rounding to binary32. Evidence is at 0x599df,
  0x59b60, 0x59bba and 0x59c06.
- `FX_InitParticle`, 0x59d78: both random velocity weight triplets draw z, y, x
  and pass x, y, z. Explicit temporaries replace compiler-dependent argument
  evaluation. Mac stores the first two draws for the last two arguments at
  0x59dd6–0x59e27 and 0x59ec3–0x59f14.
- `FX_AddCameraShake`, 0x59fec: radius and duration arguments were swapped.
  Mac passes radius from `mRadius`, then time from `mLife`, at 0x5a03c–0x5a05e.
- `FX_AddEmitter`, 0x5b372: angle and angle-delta range draws run third,
  second, first, as at 0x5b45f–0x5b501. Their final component stores retain
  their existing order.
- `FX_AddPrimitive`, 0x5a62a: the 1,800-effect limit now emits the
  `com_statmon`-gated maximum-effects warning at 0x5a89c–0x5a8f9. The cluster
  acquisition still happens before the failed allocation is reported.
- `FX_DrawAll`, 0x5a154: sight-block visibility collection uses the Mac's
  nonzero count test at 0x5a257–0x5a268, including negative counts.
- `FX_Rewind`, 0x5bda8: both removal passes are followed by scheduler
  `Clean(false, NULL)`, as at 0x5c13b–0x5c155.
- `FX_DrawScheduledEffects`, 0x5c37e: newly added effects start updating at
  the previous private count, as at 0x5c4f2 and 0x5c5a7. Existing survivors
  were previously updated twice. The two omitted free passes for effects
  removed during these updates are restored, including cluster references,
  destruction, list compaction and active counts.
- `FX_Restore`, 0x5ca50: all 13 jump-table words at 0x328c80 were decoded.
  Types are 1 particle, 2 line, 3 tail, 4 cylinder, 5 emitter, 7 oriented
  particle, 9 light, 11 flash and 12 cloud. Other nonzero types skip. Native
  had guessed a different mapping. Restored Flash vtable setup and deletion
  when a primitive template cannot be recovered. The archive temporary and
  object allocations now use native types. The existing int return API now
  returns `memFile->bytesUsed`, as at 0x5cc42, instead of pointer bits.

## Native contracts retained

The configured arm64 translation unit reports `sizeof(FxArchive)=32`,
`sizeof(Effect)=232`, `sizeof(Particle)=888`, `sizeof(Light)=336`,
`sizeof(Emitter)=976`, `sizeof(Cloud)=960` and `sizeof(FxScheduler)=24`.
Restore uses `sizeof` for every concrete allocated type. The generated backing
arrays contain 1,800 native pointers per list and 1,800 typed clusters.

Mac allocates primitives with global `new[]`, clears them, and destroys them
through `delete[]`; its deleting destructors were checked for all eight
primitive families. Native retains that family and C ABI dispatch. Linux's
per-class memory pools are not a layout reference. Scheduler allocation
continues to use `Z_MallocInternal` and `Z_FreeInternal`.

The documented WS12 full-width material ordering, equal-distance comparator
result, 16-byte `SortedEffect`, explicit origin argument, origin/axis packing
and normal-to-axis provider adapters remain. The F1 particle velocity/impact
corrections were inspected and left intact. FxPrimitives, FxScheduler, shared
headers and generated data providers were not edited.

## Verification and remaining gaps

The arm64 Release `cod2_macos_ded` target compiled successfully with
`taskpolicy -b nice -n 19` and two jobs, including a final incremental build.
Triage and constant scans completed; control flow, argument order, literals,
field access and decoded dispatch were reviewed by hand. `git diff --check`
passes. `unifdef -UCOD2_X64` output compares byte-identically with the base.
The unchanged assertion line map precedes these edits; this file contains no
`__LINE__` use.

All corrections are compiled and reviewed, not exercised in this worker.
No tests were added or edited. The existing primitive fixture needs the
`com_statmon` and `StatMon_Warning` linkage adapters coordinated with the
FxPrimitives worker; integration owns the combined fixture run. No full CI,
replay or live rendering was run.

Unchecked rows are `FX_Save`, `FX_ArchiveEffect`, `FX_GetEffectVisibility` and
the unknown-name `FX_UtilUnused`. No corresponding
Mac executable identity or verified write-side callsite was found. The
array-based server visibility entry does not prove the separate Linux
active-list visibility helper. No missing save implementation was inferred
from Linux alone. Restore retains its existing null-template guard; malformed
archives using nonfatal overflow reads remain outside the equality claim.

Private evidence and the complete per-ID ledger are outside the repository
under `recon-fx-util.noindex` and the assigned worker ledger path.

## Integrated batch verification

Integrated source 4584867 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

## Scalar writer identity follow-up, 2026-11-05

The two scalar writer rows are generic `FxArchive` members whose definitions
appear in multiple reconstructed translation units. Their earlier
“save-side-only” classification imposed a missing `FX_Save` caller requirement
on these complete scalar operations. Original Mac STABS declares
`FxArchive::WriteByte(unsigned char)` and `FxArchive::WriteInt(int)` with the
same member signatures. Linux supplied the duplicate-definition identities,
not the behavioral proof.

Exact Mac `ArchiveEffect` at 0xaa22e–0xaa248 materializes an unsigned-byte local,
passes its address and count 1, and calls `WriteData`. Native
`FxArchive_ArchiveEffect` performs that complete operation. Exact
`FxScheduler::Archive` at 0x625ac–0x625c9 materializes an integer local and passes
its address and count 4 to `WriteData`; ordered Mac source metadata attributes
the forwarding block to `FxArchive.h` line 137. Native `FxScheduler_Archive`
matches. The same integer operation occurs in `Effect::Archive` at
0xa83e0–0xa8400 and native `ArchiveInt`; its duplicate rows in the primitives
and scheduler modules were already reconciled. The new classifications rest on
these complete Mac/native scalar operations, not solely on that prior verdict
or the existence of a `WriteData` provider.

Both rows are now equal. This establishes neither `FX_Save`, `BeginWriting`,
nor the missing live-effect save wrapper. Existing writer fixes and the
nonfatal-overflow adaptation remain documented in the archive report. No
production code, fixture or previously resolved verdict changed. No new
compilation, CI, tests or replay ran; no archive runtime coverage is claimed.
Private exact-symbol evidence and the full module ledger are retained in
`recon-inline-effects4.noindex` and orchestration `units/inline-effects4.ledger.json`.


## Original save-side ownership evidence

Original 1.0 body0x8131b58 uniquely maps to original1.3 0x8133fa0. Complete operation calls effect virtual TypeID slot+0x24, writes its byte, writes effect end time field+0xbc, then dispatches virtual Archive slot+0x28. Actual original FX_Save0x8133ffe calls it at0x813407f and0x81340c1. Exact Mac FxArchive::ArchiveEffect0xaa1b2 is a different template-name serialization operation, including its writing branch0xaa208; it does not establish this effect-instance header/virtual dispatch operation. No complete corresponding Mac/native owner identified; remains unchecked.

Original 1.0 body0x8131bb6 uniquely maps to original1.3 0x8133ffe: actual archive constructor0x8122b98, BeginWriting0x8122c24, helper/scheduler archives, bolt and nonbolt effect loops through0x8133fa0, zero terminator and used-size return. Fresh Mac FX_Restore0x5ca50 establishes the distinct read-side owner with actual BeginReading at0x5cb19; existing writing primitives and template serialization do not supply the complete save operation. No corresponding complete Mac/native save owner identified. Bounded original call/reference search is retained as search evidence only, never as a closure argument.

These original executable identities refine the unresolved contracts. No verdict or source change, compilation, tests, CI, replay or runtime exercise. Private evidence remains outside Git in recon-fxarchive9.noindex.

## Original helper provenance — fx9

The original empty slot immediately preceding the uniquely mapped kill-all-effects owner is at 1.0 0x813117a / 1.3 0x81335c2. Its reconstructed FX_UtilUnused label still does not recover a meaningful original name or complete Mac/native operation. The row remains unchecked.

Original 1.0 0x81315a6 uniquely maps to original 1.3 0x81339ee. Its complete body checks the active sight-blocker count, normalizes the requested segment, applies the minimum trace distance, then traverses non-bolted and bolted effect lists. Each flagged object invokes virtual slot +0x18 GetVisibility and multiplies its returned value into the result. Fresh exact Mac FX_GetServerVisibility 0x596c6 instead traverses cached 20-byte visibility records and performs point/radius/value tests. Native retains that cached-record owner. A shared segment setup is not the full active-list/virtual-call/state-timing contract, and no actual original query consumer establishes an approved replacement boundary. The row remains unchecked.

The retained direct-call scan finds the positive channel-copy-to-curve-reference chain; raw absolute address references add only unwind records. Those bounded searches are not evidence of semantic absence and close no row. No source or prior resolved verdict changed, no new defect was established, and no compilation, tests, CI, replay or runtime exercise ran. Original bodies, disambiguating neighbors, decoded table and fresh exact Mac candidates are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-fx9.noindex/`.
