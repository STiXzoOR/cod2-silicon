# EffectsCore/FxScheduler module reconciliation

Updated 2026-11-05.

**58/59 reconciled: 44 equal, 6 fixed, 8 intentional, 1 unchecked.** Mac 1.3 i386 is the authority. Linux supplied helper names only. Every Mac body and identified inline helper was read, including the dispatch table at 0x328d40. The legacy source bodies are unchanged.

## Corrections

**Random roll selected the wrong flag and axes.** Mac 0x6108b tests spawn bit 0x100. The rotation at 0x610c0 writes local axis[1], and the cross product at 0x610d3 writes axis[2]. Native tested bit 1, kept the old axis[2], and replaced axis[1] with the cross product.

**Simple effects faced the wrong direction.** Mac's simple overload at 0x61862 constructs rows `{0,0,1}`, `{1,0,0}`, `{0,1,0}`. The native null-axis fallback used identity. History commit acccfb736 added that fallback to support `FX_PlaySimpleEffect`, which now explicitly passes a null axis and bolt to the common native scheduler. No intentional orientation decision was documented. The fallback now supplies the Mac rows; explicitly supplied axes retain their values. The existing delayed-effect fixture had pinned identity, so its existing assertion was corrected without adding a case.

**Skipped primitives changed delayed-effect seeds.** Mac 0x61609 advances the seed offset by 0x369d035 for every primitive, including culled primitives and zero spawn counts. Native `continue` statements bypassed that update. Native now derives the same offset from the primitive index. Start-time and seed additions preserve the 32-bit `addl` results at 0x61518 and 0x61539 through unsigned arithmetic.

## Native contracts and retained differences

`ScheduledEffect` uses a typed native-width list link and a native-sized allocation. `FxUtil` reads that same field through `SCH_NEXT`. The constructor, list prepend, removal and cleanup agree with the Mac list contract. The native C++ ABI wrappers delegate `__Znam` and `__ZdaPv` to global `new[]` and `delete[]`; the Linux pool helpers are not a missing Mac backend. The Mac static initializer at 0x60fb6 is empty. Existing native bolted-axis copying, pointer-valued bolt frames, defined primitive initialization, and the corrupt-template guard remain.

The forward/up and direction overloads are implemented by native macOS `Fxexport` wrappers. Their copied rows, cross product and `MakeNormalVectors` calls match 0x6178e and 0x61802. The simple overload is collapsed into the common scheduler's null-axis fallback.

The archive's ordinary nonnegative-count record order, validation, prepend/delete decisions, and traversal of the global scheduler match 0x62560. The Mac record omits batch index and list link; native restored records retain their defined zero initialization. Native keeps the existing signed count loop, which skips negative counts. Mac 0x625a0/0x62624 instead iterates until a 32-bit counter equals the supplied count. That malformed-input domain is intentionally not reproduced: `FxArchive_ReadData` has no error result, and a nonfatal `MemoryFile` overflow can leave its local byte unwritten. Normal `MemFile_InitForReading` sets fatal overflow handling, and `Com_Error(1)` longjmps; allocation uses throwing native `new[]`. The independent `FxUtil` worker owns its restore-storage correction. No full restore equivalence is claimed here.

The raw-pointer assignment helper is inlined in the Mac bolt archive's negative-handle branch. After releasing a nonnull old frame, 0xa1b22 increments address zero. Native releases and clears the pointer with defined behavior. This existing adaptation is retained; provider code is unchanged.

One row remains unchecked: Linux `FX_SchedulerUnused` at line 633 is an empty function with an unknown original name. No Mac symbol or identifiable caller was established. It is not equated with the unrelated empty static initializer.

## Verification and limits

Configured a local arm64 Release build and linked `cod2_macos_ded` at low priority with two jobs. Both assigned translation units also compiled separately with the actual client and dedicated commands from the local compilation database. The existing `scheduled_fx` fixture passed under ASan/UBSan in stock and `COD2_CODX` modes. The existing dedicated/client parser fixtures passed under ASan/UBSan. No new cases or tests, full CI, replays, or integration builds were run.

The existing queue fixture exercises delayed native allocation, widened links, simple-effect axis storage and cleanup. Random-roll creation, skipped-primitive seed changes, integer wraparound, decal interpolation and archive execution were compiled and reviewed but not exercised. The OFF guard checker reported ten comparisons and zero mismatches across the two changed source files; neither file uses `__LINE__`.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-scheduler.noindex/`. Per-ID verdicts are in the private `fx-scheduler.ledger.json` supplied to the orchestrator.

## Integrated batch verification

Integrated source 5bf663a passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.


## Original helper provenance — fx9

The original empty slot is located at 1.0 0x8128738 / 1.3 0x812ab58 immediately before the MediaHandles GetEffect/GetHandle pair; the latter is independently mapped by its complete body. Its name and purpose remain unknown. Identical empty functions elsewhere, including Mac global initializers, are not a semantic identity. The row remains unchecked.

The retained direct-call scan finds the positive channel-copy-to-curve-reference chain; raw absolute address references add only unwind records. Those bounded searches are not evidence of semantic absence and close no row. No source or prior resolved verdict changed, no new defect was established, and no compilation, tests, CI, replay or runtime exercise ran. Original bodies, disambiguating neighbors, decoded table and fresh exact Mac candidates are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-fx9.noindex/`.
