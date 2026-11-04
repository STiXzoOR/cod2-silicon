# EffectsCore/FxUtil module reconciliation

Updated 2026-11-05.

132 of 138 rows reconciled: 82 equal, 16 fixed, 34 intentional. Six Linux-only
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

Unchecked rows are `FX_Save`, `FX_ArchiveEffect`, `WriteByte`, `WriteInt`,
`FX_GetEffectVisibility` and the unknown-name `FX_UtilUnused`. No corresponding
Mac executable identity or verified write-side callsite was found. The
array-based server visibility entry does not prove the separate Linux
active-list visibility helper. No missing save implementation was inferred
from Linux alone. Restore retains its existing null-template guard; malformed
archives using nonfatal overflow reads remain outside the equality claim.

Private evidence and the complete per-ID ledger are outside the repository
under `recon-fx-util.noindex` and the assigned worker ledger path.
