# xanim/xanim module reconciliation

Updated 2026-11-04.

**133/147 reconciled: 118 equal, 11 fixed, 4 intentional; 14 unchecked.** Mac 1.3 i386 is the authority. Every assigned row was evaluated. Nontrivial exported functions were compared by hand, including the complete blend, cache, update and notify bodies and the compiler's repeated inlined recursion. Linux was used only for helper names and intent.

## Corrections

**Server note prediction used unsigned cycle counts.** Mac 0x4272e and 0x427d6 sign-extend the predicted 16-bit cycle count before comparing predicted and current time. Native promoted an unsigned short, which changed the decision at the signed 0x8000 boundary. Both leaf and sync prediction now use the Mac conversion.

**Several unordered float comparisons differed.** Mac 0x38f77 and 0x3b738 accept unordered candidates when selecting a weighted descendant; 0x3b66f takes the descending blend branch for unordered weights. Mac notify insertion at 0x40791/0x407c0 shifts entries on unordered fractions. The next-note scan at 0x3ef58 and 0x420d9 rejects only a strictly later current time. Native now preserves those comparisons, including the corresponding note acceptance gates. Full notify rows remain unchecked for the separate storage-dependent case below.

**Absolute delta lost zero accumulation.** Mac 0x39a17–0x39a3b adds positive zero to the scaled rotation and sampled translations. Native had copied those values, preserving negative zero. The additions are restored.

**Time-index lookup used clamped linear searches.** Mac specializations at 0x3897c and 0x38ae6 use binary interval searches from a time-based estimate. Native now searches by interval, preserving its endpoint clamps/extrapolation, the dense-frame shortcut and interpolation subtraction/division order. Exact time 1 and zero-frame clips take the final-sample branch before search (Mac 0x395b0–0x39620; leaf time-1 dispatch at 0x3c231–0x3c5d5). Compressed tracks store size+1 index/frame samples and use intervals through size-1. No meaningful Mac read beyond that storage is inferred. The four Linux helper identities sharing these paths are recorded against those Mac specializations.

The existing 64-bit animation entries, tree header and sized allocation/free callbacks are retained. Those four allocation-size verdicts are intentional ABI adaptations. Cache handles remain owned by the tree; stale anim-to-model strings are released before replacement, and clearing/freeing a tree releases each cached string using boneCount+16. The notify pool capacity 128, info pool capacity 4096, masks, flags, error strings and SSE constants were checked. No new registration or data table was introduced.

## Unchecked entries

Five notify rows remain unchecked: `XAnimGetNextNotifyIndex`, `XAnimSetNotifyIndex`, `XAnimProcessServerNotify`, `XAnimProcessClientNotify` and `XAnimGetNotifyFracServer`. When no next note is found, Mac 0x3ee7a–0x3ee8e / 0x42048–0x42059 derives a short index from a null pointer minus the notify-table address. Native explicitly uses -1. The loader appends an end sentinel for ordinary clips, but a full equivalence claim for empty/exhausted tables and address-dependent results is not established. Non-finite dispatch beyond the corrected gates is also unproven. The Mac wrapped-prefix stride-two behavior at 0x4115d remains preserved.

Nine Linux-only routines lack a matching Mac text symbol or a uniquely established complete inline identity: `DObjUpdateServerOldTime`, `XAnimClearChildGoalWeights`, `XAnimCloneAnimTreeOldState`, `XAnimCopyAnimTreeNoNotify`, `XAnimCopyInfoNoNotify`, `XAnimDisableLeakCheck`, `XAnimFreeAnims`, `XAnimSetCompleteGoalWeightKnob` and `XAnimSetGoalWeightKnob`. Their absence was not converted into equality.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiles with `taskpolicy -b nice -n 19` and two jobs. Existing animation/root-motion/blending/notify/skeleton/surface fixtures and all seven existing synchronized notification cases pass unchanged under ASan/UBSan. No new tests, full CI or replays were run. The signed-cycle boundary, signed-zero and NaN corrections are compiled and reviewed but are not specifically exercised by those fixtures.

The `COD2_X64=OFF` preprocessed source is byte-identical to the starting commit. Its SHA256 is `a50b4e09393ca537a0de4195cc2bef91458fe3a738e0541adcb4ce727fdf7963`. Only native branches changed. Full-binary disassembly misaligned the XAnimInit prologue; its exact-symbol disassembly was used instead.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-core.noindex/`. The complete 157-row private ledger is `xanim-core.ledger.json` in the orchestrator's units directory.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
