# WS48 — Native keyed-command masks

2026-10-07; base `0e46885152b77b5bd380c1b356400fc7d3878f17`.

## Result

FA01's shifted native key-mask table is corrected. Width 13 now masks with
8191, and width 32 with 4294967295. All 32 keyed-field probe widths match the
private Mac 1.3 reference in stock and CoD2x compilation modes, without
`--expect-difference`. This removes out-of-width key bits from decoded values;
it does not establish an observed player action or complete network fidelity.

## Cause and change

`build/native_gen/data32.c` carries a 33-entry initializer that omits 8191,
shifts the following masks and ends in zero. The native typed-data generator
faithfully preserved that incorrect input. Fixing the separate assembly table
did not change the runtime's generated C table.

`tools/datagen/generate.py` now emits low-N-bit masks for the native table's
LP64 branch, checking the layout and every mask against the independently
selected Steam Mac value reference. The original i386 initializer, padding,
symbols and relocations remain intact. Only `data_native.c` changed in the
five-file committed snapshot. The parser fuzz fixture now uses the corrected
native masks; its old claim that the retail table also skipped width 13 was
incorrect.

## Verification

- Added a public, private-binary-free regression that compiles the committed
  native snapshot and reads its actual Mach-O mask object. Before the fix it
  failed first at index 13 (16383 instead of 8191). After regeneration it passes.
- Full `tools/fidelity` unit suite: 33 tests passed.
- Full typed-data round-trip: all original i386 section bytes, relocation
  records and symbol addresses/bindings match; arm64 full/native and BSS probes
  pass; all 14 generated text outputs are identical on a second generation.
  All 17 recovered LP64 object instances match their value reference.
- Compiled the corrected table separately with the actual stock and CoD2x
  Release engine flags from WS45, remapping source/output paths to this worktree
  and private evidence folders. The existing production-function message probe
  reads those built objects and reports **0/32 differences in each mode**.
  These are focused compilation/probe results, not two new full linked builds.
- Production message-parser ASan/UBSan fuzz target: all four saved regressions
  pass in both modes; seed-1, 30-second campaigns complete without crashes
  (16,128 stock runs; 15,104 CoD2x runs). This is bounded fuzz coverage, not an
  exhaustive protocol proof.

Evidence is private at
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/`.
The first generator attempt rejected object length 156 because it includes
24 bytes of zero intersymbol padding beyond the 132-byte STABS table. The
generator now validates and preserves that tail; no reference mismatch was
ignored. Its failure log remains available.

No game session, live packet interoperability run, full composed merge gate or
Linux multilib binary-equality run was performed by this unit. WS45 owns the
live test session. The composed branch must rerun the merge gates after this
change and the other fidelity fixes are integrated.
