# F2: dropped weapon item count

The combat smoke reached a real disconnect after death: `CG_Item` rejected the
dropped Enfield at index 6. Process exit zero did not mean successful gameplay.
The fixed Mac item bound was missing from native generated data.

## Cause and reference

The original `bg_numItems` is a four-byte integer containing **131** in both
owned Mac binaries: STABS Mac at `0x326230`, Steam at `0x30051c`. The named
`bg_itemlist` spans 132 original 44-byte entries, including its terminal entry.
Mac `CG_Item` at `0x164565`–`0x164572` compares the item index against this
imported bound. Mac weapon setup at `0x7ee82`–`0x7eecc` fills an item without
changing the count.

Native input instead supplied a one-byte zero placeholder. The datagen snapshot
preserved it. F1 commit `11a9e6e` removed the artificial count growth in weapon
setup to match Mac behavior, exposing the missing initial value. Both live build
objects contained zero; the later event, prediction and snapshot changes did
not modify this data or the item bounds check. WS16 had already recorded the
placeholder's insufficient storage.

## Repair

The generator validates a unique four-byte `int` debug type, the known zero
placeholder, and the named value reference. It emits initialized four-byte
storage containing 131 on LP64. Recovery metadata feeds the existing compiled
scalar verification. The original i386 initializer remains unchanged.

Only `build/lp64_gen/literals_native.c` changes in the five-file snapshot.
`typed_types.h` and all other snapshot files are unchanged. No engine function,
item bounds check, weapon count update or oracle changes.

## Verification

All commands ran sequentially under `taskpolicy -b nice -n 19`, with at most one
compiler. Private logs are under
`~/Library/Application Support/CoD2-native-decomp/F2-dropped-item.noindex/`.

- Before repair, the compiled snapshot regression failed with `0 != 131`.
  The behavioral fixture separately failed under ASan on the production
  `CG_Item` four-byte read past the one-byte generated object.
- `python3 -m unittest discover -s tools/datagen -v`: **34 tests pass**.
  New coverage rejects wrong/ambiguous types, wrong storage, relocations,
  contradictory values and reference extents shorter than four bytes. It
  checks compiled arm64 scalar bytes and freestanding x86_64 ELF compilation.
- `python3 tests/camera/run_items.py --compile-commands <stock-or-codx-engine-db>
  --output <private-output>`: **both modes pass ASan/UBSan**. The fixture uses
  the unchanged production `CG_Item` body and the actual generated count and
  import definitions; it does not mock the count. It checks dropped items 6 and
  130 reach rendering, unregistered and hidden item behavior, and index 131
  causes the original error even when hidden. Renderer endpoints are synthetic.
- `sh tools/datagen/roundtrip_test.sh <owned-stabs-binary> <private-output>`:
  **pass**. Original and native i386 blob bytes, relocations and symbols match;
  arm64 full/native and BSS probes pass; all 19 recovered scalar objects match
  the named Steam values; 14 text outputs are deterministic on regeneration.
- `python3 tools/datagen/snapshot.py <private-roundtrip-output> build/lp64_gen`:
  **all five files byte-identical**.
- `unifdef -D__SIZEOF_POINTER__=4` gives an exact source match for the changed
  snapshot file against `3ff2eae`; no engine source changed. `git diff --check`
  passes.

After integration, the orchestrator rebuilt both client and dedicated targets
in stock and CoD2x. The exact Enfield/fire/reload/frag/smoke/death/respawn sequence
now completes in both clients without the item-index error or disconnection.
Death and respawn captures were visually inspected: corpse and dropped equipment
render, then normal first-person play resumes. Both runs exit zero. The inherited
missing `beep.wav` warning remains; no unknown command or sanitizer marker occurs.
Private `live-stock` and `live-codx` evidence is beside the fixture logs. The VM
was suspended during each native run and its running state restored afterward;
no guest command or authentication change was used.

The five-lens simplify pass consolidated identical generator recovery bookkeeping.
All 34 datagen tests and fresh generation pass afterward; the five snapshot files
remain byte-identical, so no game binary changes follow from that consolidation.
The blob round-trip is not full Linux game-binary identity, and this bounded
repair does not establish complete multiplayer parity.

## Review resolution

The focused review completed with one P3 fixture finding and no production
defect: `/tmp/compound-engineering-501/ce-code-review/20261026-091417-dd31b6b4/`.
Correctness and an independent Claude Opus 5.5 pass covered the change. Finding
#1 is applied: the positive recovery fixture now starts with a BSS placeholder
and checks that the original remains BSS while the recovered copy becomes data.
All 34 datagen tests pass after this change (`review-fix-tests.log`). No actionable
review finding remains; this test-only repair does not change generated bytes.
