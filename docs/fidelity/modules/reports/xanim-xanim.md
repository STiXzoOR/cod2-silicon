# xanim/xanim module reconciliation

Updated 2026-11-05.

**139/147 reconciled: 118 equal, 14 fixed, 7 intentional; 8 unchecked.** Mac 1.3 i386 is the authority. Every assigned row was evaluated. Nontrivial exported functions were compared by hand, including the complete blend, cache, update and notify bodies and the compiler's repeated inlined recursion. Linux was used only for helper names and intent.

## Corrections

**Server note prediction used unsigned cycle counts.** Mac 0x4272e and 0x427d6 sign-extend the predicted 16-bit cycle count before comparing predicted and current time. Native promoted an unsigned short, which changed the decision at the signed 0x8000 boundary. Both leaf and sync prediction now use the Mac conversion.

**Several unordered float comparisons differed.** Mac 0x38f77 and 0x3b738 accept unordered candidates when selecting a weighted descendant; 0x3b66f takes the descending blend branch for unordered weights. Mac notify insertion at 0x40791/0x407c0 shifts entries on unordered fractions. The next-note scan at 0x3ef58 and 0x420d9 rejects only a strictly later current time. Native now preserves those comparisons, including the corresponding note acceptance gates. Full notify rows remain unchecked for the separate storage-dependent case below.

**Absolute delta lost zero accumulation.** Mac 0x39a17–0x39a3b adds positive zero to the scaled rotation and sampled translations. Native had copied those values, preserving negative zero. The additions are restored.

**Time-index lookup used clamped linear searches.** Mac specializations at 0x3897c and 0x38ae6 use binary interval searches from a time-based estimate. Native now searches by interval, preserving its endpoint clamps/extrapolation, the dense-frame shortcut and interpolation subtraction/division order. Exact time 1 and zero-frame clips take the final-sample branch before search (Mac 0x395b0–0x39620; leaf time-1 dispatch at 0x3c231–0x3c5d5). Compressed tracks store size+1 index/frame samples and use intervals through size-1. No meaningful Mac read beyond that storage is inferred. The four Linux helper identities sharing these paths are recorded against those Mac specializations.

The existing 64-bit animation entries, tree header and sized allocation/free callbacks are retained. Those four allocation-size verdicts are intentional ABI adaptations. Cache handles remain owned by the tree; stale anim-to-model strings are released before replacement, and clearing/freeing a tree releases each cached string using boneCount+16. The notify pool capacity 128, info pool capacity 4096, masks, flags, error strings and SSE constants were checked. No new registration or data table was introduced.

## Unchecked entries

The two remaining notify helper identities are now resolved against their complete inline operations. `XAnimGetNextNotifyIndex` comprises the scan at Mac 0x3ef45–0x3ef71 / 0x420c1–0x420f6 and the short-index conversion at 0x3ee7c–0x3ee87 / 0x4204a–0x42052. `XAnimSetNotifyIndex` includes the exact-time-1 early exit, nonleaf notify-child selection and index store at 0x42012–0x42059. Existing native code implements these operations through `XAnimGetNextNotifyInfo` and the corresponding owner blocks. The scan starts at time 2, keeps the first minimum qualifying time, accepts unordered current time, and rejects unordered note time.

Both helper verdicts are intentional because native explicitly stores -1 when no candidate exists. Mac instead derives a short from null minus the table address. This preserves the existing owner policy and does not claim address-dependent ILP32 equality. Exact loader 0xba3cb–0xba415 appends the time-1 end sentinel for ordinary clips; empty or exhausted tables follow the explicit native policy. The previously reconciled server/client notify and fraction-owner verdicts are unchanged.
Nine Linux-only routines lack a matching Mac text symbol or a uniquely established complete inline identity: `DObjUpdateServerOldTime`, `XAnimClearChildGoalWeights`, `XAnimCloneAnimTreeOldState`, `XAnimCopyAnimTreeNoNotify`, `XAnimCopyInfoNoNotify`, `XAnimDisableLeakCheck`, `XAnimFreeAnims`, `XAnimSetCompleteGoalWeightKnob` and `XAnimSetGoalWeightKnob`. Their absence was not converted into equality.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiles with `taskpolicy -b nice -n 19` and two jobs. Existing animation/root-motion/blending/notify/skeleton/surface fixtures and all seven existing synchronized notification cases pass unchanged under ASan/UBSan. No new tests, full CI or replays were run. The signed-cycle boundary, signed-zero and NaN corrections are compiled and reviewed but are not specifically exercised by those fixtures.

The `COD2_X64=OFF` preprocessed source is byte-identical to the starting commit. Its SHA256 is `a50b4e09393ca537a0de4195cc2bef91458fe3a738e0541adcb4ce727fdf7963`. Only native branches changed. Full-binary disassembly misaligned the XAnimInit prologue; its exact-symbol disassembly was used instead.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-core.noindex/`. The complete 157-row private ledger is `xanim-core.ledger.json` in the orchestrator's units directory.

## Integrated batch verification

Integrated source `cf0fc8c` with cross-module evidence commit `e785f19` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `f763fd9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.

## Remaining-helper follow-up — 2026-11-05

Mac STABS confirms the eight-byte note record, byte note count, signed-short index, entry union and tree/info layouts. Exact-symbol disassembly was used for the scan and setter; no production refactoring was needed. Two helper rows changed from unchecked to intentional. All previously resolved evidence remains unchanged.

The other nine helper rows remain unchecked. `DObjInitServerTime` at 0x3b554 updates one tree and does not perform the additional cross-object old-state clone required by `DObjUpdateServerOldTime`. `XAnimCloneAnimTree` at 0x3bb04 copies the full info, maintains notification references and removes absent destination nodes; that does not establish old-state-only or no-notify copy operations. `XAnimClearTreeGoalWeightsStrict` recursively clears descendants, so it is not the direct-children-only helper. The knob-all owner does not establish the two distinct wrapper contracts. `XAnimFreeList` at 0x386a0 frees debug names, but does not establish the absent sized allocation callback wrapper. Leak-check has a zero-valued exception-metadata name without an established executable state-write path. None of those partial operations or metadata records was promoted to a complete helper verdict.

Configured stock/CoD2x Release objects compiled. Existing full xanim/model/skeleton fixtures and all seven synchronized notification cases pass both modes under ASan/UBSan. The full fixture initially failed its existing model-name backslash assertion; the approved expectation correction is recorded in the xmodel report, with failed logs retained. No new tests, cases, assertions, CI or replays were added or run. No production source changed, so legacy text and actual diagnostic line expansions are unchanged. Empty/exhausted/nonfinite notify states were reviewed, not specifically exercised.

Private follow-up evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-animation-remaining.noindex/`.


## Original-address and semantic follow-up (inline-effects4)

Original Linux 1.3 executable bodies were recovered independently of reconstructed names, using the Linux 1.0 database to propose identities and the 1.3 instruction bodies plus `.eh_frame_hdr`/FDE ranges to establish boundaries. These anchors supply identity and intent only: XAnimFreeAnims at0x80bcca8, XAnimCloneAnimTreeOldState at0x80beeea, DObjUpdateServerOldTime at0x80c0ffc, the complete/ordinary knob wrappers at0x80c1c04/0x80c1df6, ClearChildGoalWeights at0x80c1e76, and the no-notify info/tree copies at0x80c2d0a/0x80c2d6a. No verdict changes follow from these Linux bodies.

The no-notify operation copies all40 bytes then clears notifyChild/name and sets notifyIndex to-1. A Mac text semantic search collected77 index-reset candidates across17 owners, including unrelated code; this is candidate discovery, not exhaustive semantic equivalence. The exact Mac XAnimCloneAnimTree body at0x3bb04 rejects the tempting match: its reset at0x3bca4 occurs before the40-byte copy at0x3bbfe–0x3bc54, which restores the source notification fields and then retains notifyName at0x3bc57–0x3bc6a. Its other reset is a free-path operation. The missing no-notify copy cannot be closed from this regular clone.

The original knob wrappers clamp the weight, clear sibling weights through a separate helper, and call the corresponding goal setter with notifyType zero. They are not strict aliases to the setters or to KnobAll. OldState copies selected old-time/state fields between two trees; the ordinary Mac full-state clone is not that operation. Original direct-call scans link only OldState to DObjUpdateServerOldTime and CopyInfoNoNotify to CopyAnimTreeNoNotify within this group; absence of outside direct calls supplies no closure. A complete Mac operation remains required for every retained unchecked row. No source changes, compilation, tests, CI, or runtime execution occurred.

## Original leak-state write and source provenance

The anonymous Linux 1.0 body at 0x80ba7b0 maps to original 1.3 0x80bcd88–0x80bcd94 and writes byte one to its leak-check global. This supplies a concrete original `XAnimDisableLeakCheck` state-write anchor, but a complete primary Mac owner remains unidentified. Related initialization or exception metadata cannot establish the missing write. The eight previously anchored helper bodies retain their existing complete-operation gaps. Targeted PPC source/owner additions concern initialization, precache and notification owners and do not establish those missing operations. All nine stay unchecked; notification/storage adaptations remain intact.

Private evidence is in `recon-provenance-animation10.noindex`. No source changes, compilation, tests, CI, replay or runtime exercise ran.

## Complete ordinary-knob operation (anim19)

`XAnimSetGoalWeightKnob` is fixed through the complete initial target phase of actual Mac/native `XAnimSetCompleteGoalWeightKnobAll`. The original 127-byte wrapper at 0x80c1df6–0x80c1e74 establishes the clamp and two-provider operation. Mac first clamps weight at 0x3ef81–0x3ef9f, then computes the current-weight sibling blend and clears siblings, calls the complete internal target setter at 0x3f061 with notify type zero, allocates missing zero-goal parents, and performs all optional target restart branches. These effects join at 0x3f0ed/0x3f0f8 before the root-limited ancestor loop. Native has the same complete phase, consumed by actual animation callers.

This classification inherits the already repaired internal setter's unordered descending-weight and descendant-selection gates at 0x3b66f and 0x3b738. The wrapper preserves unordered input, so those repairs are within its operation; no new defect was found. It is not an alias of whole KnobAll, and its void result does not acquire the enclosing owner's return contract. Later ancestor propagation is excluded. The complete-knob wrapper's weight-one ancestor policy and immediate-child-only clearing remain separate unchecked operations.

The mapping retains valid tree/storage and existing native notification ownership. It does not claim allocation-exhaustion continuation equality after a returned fatal Com_Error: Mac resumes with null storage while native supplies defined g_xAnimInfo[0] storage. That existing adaptation remains unchanged. No standalone exported ABI, literal inline ancestry, malformed-tree behavior, FP flags/traps or NaN payload equality is claimed. Full original providers and fresh exact Mac bodies were independently reviewed in units/review-provenance-anim19.md; private evidence is in recon-provenance-anim19.noindex. No source changed. No compilation, tests, CI or replay ran. Inherited corrections remain compiled and reviewed, not runtime verified.


## Distinct historical-state clone/update contracts (dobj40)

XAnimCloneAnimTreeOldState remains unchecked. Its complete original0x80beeea–RET0x80bf018 reconciles slot existence and resets newly allocated info, then transfers five state fields: old time/count into current time/count plus goal weight/rate/time. Other existing destination fields and notification ownership survive. Actual Mac XAnimCloneAnimTree0x3bb04, called by both server and client corpse lifecycle, replaces all40info bytes and releases/retains notification references. It preserves current-to-current and old-to-old fields; it is not that selective historical-state transfer. Native's full clone is preserved, without using an arbitrary byte slice as a substitute.

DObjUpdateServerOldTime also remains unchecked. Complete original0x80c0ffc–RET0x80c106a first updates the separate server object's tree, then transfers historical state from the source object's tree. Full actual Mac DObjInitServerTime0x3b554–RET0x3b59c accepts one object, updates that same tree and returns. Neither this one-tree update nor the full corpse clone supplies the ordered two-tree operation. No hypothetical call composition or oldTime=currentTime restriction closes the gap.

Two focused unchecked refinements, zero closures/new defects or source changes. Complete originals and fresh exact Mac providers/real lifecycle callers, with current source hashes, are retained in recon-provenance-dobj40.noindex. Existing resolved animation and native storage adaptations remain untouched. No compilation, tests, CI, full build, replay or runtime verification.
