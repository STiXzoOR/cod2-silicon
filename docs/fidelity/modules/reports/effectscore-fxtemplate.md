# EffectsCore/FxTemplate module reconciliation

Updated 2026-11-05.

**76/77 reconciled: 45 equal, 28 fixed, 3 intentional; 1 unchecked.** Mac 1.3 i386 is the authority. Every executable entry and its inline parser blocks were read from exact-symbol disassembly. Linux supplied names only. The legacy OFF bodies are unchanged.

## Corrections

**Several ranges were reversed.** Mac `ParsePrimitiveInternal` at 0x5f778 stores first/second values as min/max for life, delay, density, variance, height, angles and origin2. Native had reversed these. The six-value bbox parsers at 0x5ff85/0x6004f alias the output triples and retain the last triple; native now preserves that quirk. Bounding-box checks run before channel groups and migration, and a negative spawn cull radius fails before storing it.

**Channel parsing had invented behavior.** Mac 0x5e8d0 recognizes `curve`, `scale`, `start`, `end`, `parm`/`parms` and `flag`/`flags`. Unknown channel keys print and continue, failed conversions are ignored, and empty groups succeed. Scalar endpoints sort, RGB endpoints retain their order, and compatibility data is marked only by start/end/parm/flags. Native now follows those rules and keeps channel names in the subgroup dispatcher. The separate RGB curve entry at 0x5e0a8 was missing and now exists. The Mac migration quirk that gives channel 1 one dimension is retained.

**Random velocity curves used the wrong divisor.** Mac 0x5ed92 normalizes both endpoint curves by the shared maximum while storing a unit scale on the random channel. Native divided the second endpoint by one. Acceleration at 0x5f244 now uses the scaled denominator and Mac's multiplication order. The integer truncation/absolute-range helper models `cvttss2si`'s INT_MIN sentinel with defined C operations, and maximum selection follows the SSE order.

**Rotation decay used incorrect factors.** Mac 0x5e16e uses previous times `1-step` for a positive step below one, previous times zero for a step at least one, and leaves previous unchanged otherwise. This also applies when the initial scale is zero. Native now follows those branches. Flag parsing at 0x5e5c4 also retains Mac's token-length-plus-one advancement instead of skipping whitespace independently.

Both flag-name/mask tables were compared against binary data at 0x36a800/0x36a760. The group-flag initializer at 0x328cc0 matches, including its isolated ASCII zero. The short-list unknown-token return quirk in `ParseGroupFlags` is retained.

## Layout and input contracts

Native layouts were measured with production arm64 flags: `PrimitiveTemplate` 816 bytes, `FxChannel` 16, `MediaHandles` 16, `GPValue` 40, `GPGroup` 96, and pointer-free `BackCompatibleParameters` 1536. The loader uses `sizeof(PrimitiveTemplate)` and a zeroing Hunk allocator; compatibility scratch remains exactly 0x600 and is initialized before parsing. Parsed channels use typed native storage. The existing decompiler-expanded public ParseChannel signature is retained to avoid a shared-header change.

Dedicated material parsing intentionally validates names without registering or storing renderer handles; the client follows Mac's renderer-exists check and registration arguments 3/6. The dedicated build omits `FX_RegisterMaterial`. The existing dedicated model provider calls `XModelPrecache` with Hunk allocators.

Mac ignores curve-row conversion results and its curve allocator dereferences both first and last keys even for a zero count. Temp Hunk memory is not generally zeroed. Native therefore defines partial-row slots as zero and leaves the existing curve unchanged for an empty list. Float-range parsing rejects sscanf EOF instead of reading unwritten locals, and flag scratch includes room for its NUL. These are intentional storage adaptations. General malformed text, null parser values, oversized group-flag tokens, extreme allocation counts and invalid linked lists have no established safe retail contract; equivalence is not claimed there.

`FX_PrimTypeIsEmitter` remains unchecked: it has only a Linux name, no identified executable Mac identity, and no native definition or caller.

## Verification and limits

Both owned files compile with native arm64 client engine flags, and the arm64 Release dedicated target builds. Existing `tests/platform/run_dedicated_fx.sh` passes with the real parser, 450 ms lifetime, caching, channel defaults and media growth. Existing `tests/lp64/game/run.py --test fx_channels` passes RGB/scalar curve dimensions and scale ranges. The fixtures were used unchanged; no new per-fix tests were written.

The focused fixtures do not exercise all corrected range, random endpoint, recurrence or malformed-input branches; those changes are compiled and reviewed only. No full CI or replay was run by this worker. Five OFF preprocessing configurations per changed source match the base, and `git diff --check` passes. Original source layout-assert line expansions are unchanged.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-fx-template.noindex/`.
