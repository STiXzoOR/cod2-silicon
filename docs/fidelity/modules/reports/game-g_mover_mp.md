# game/g_mover_mp module reconciliation

Updated 2026-11-04; baseline `7eebadb`.

**12/12 reconciled: 10 equal, 2 fixed.** All seven Mac exports were compared across
their complete bodies and every literal was decoded from the binary. The five Linux-only
helpers are inlined on Mac and were compared at their inlined sites. Linux 1.3 was read
only. The legacy source is unchanged.

## Correction

The fatal hintstring overflow used truncated text. It now matches Mac at 0x2e13f8,
including the leading 0x15 byte, the full sentence and the limit argument. Successful
behavior does not change.

## Function status

| Function | Status | Mac location |
|---|---|---|
| `G_CreateRotationMatrix` | equal | inlined, 0x1ba689-0x1ba6d3 |
| `G_MoverPush` | equal | inlined, 0x1baecf-0x1bb2be |
| `G_MoverTeam` | equal | 0x1badb2-0x1bb723, 2,418 bytes |
| `G_RotatePoint` | equal | inlined, 0x1ba72f-0x1ba7ae |
| `G_RunMover` | equal | 0x1bb724-0x1bb76f, 76 bytes |
| `G_TestEntityPosition` | fixed | inlined at each position test |
| `G_TransposeMatrix` | equal | inlined, 0x1ba6d8-0x1ba701 |
| `G_TryPushingEntity` | equal | 0x1ba63c-0x1badb0, 1,909 bytes |
| `trigger_use` | equal | 0x1ba624-0x1ba62f, 12 bytes |
| `trigger_use_shared` | fixed | 0x1ba444-0x1ba622, 479 bytes |
| `trigger_use_touch` | equal | 0x1ba630-0x1ba63b, 12 bytes |
| `use_trigger_use` | equal | 0x1ba43e-0x1ba442, 5 bytes |

`G_TestEntityPosition` needed no new change; its fixes landed in the 2026-10-24 batch
(`docs/macos-port/reports/2026-10-24-F1-mover.md`), which also added `HINT_INHERIT`.

## Retained original behavior

The hint table ends in a NULL entry and Mac walks through it (0x1ba5a6-0x1ba5c5).
`I_stricmp` has no NULL guard (0x29176-0x2917c), so an unknown `cursorhint` value faults
on both Mac and native. This batch does not invent a replacement policy. Any stock or
custom map using an unknown value would need one, and that would be a deliberate
difference.

## Verification and limits

`tests/lp64/game/f1_mover.c` gains two groups. `hint_strings` covers allocation, reuse
and the overflow diagnostic; it fails on `7eebadb` only at the diagnostic text. `jitter`
is a control that pins the search order and old-position fallback through nine outcomes
plus the fully blocked case; it passes before and after, confirming the existing code. Both pass in stock and CoD2x
under ASan and UBSan with assertions on.

No replay was rerun: the only source change is fatal diagnostic text, so the 2,000-frame
replays recorded for `game/g_missile_mp` remain the runtime baseline. They are not a fresh
replay of this build. Whole-project parity with the original is not claimed.

The steps of the macOS CI job pass locally, apart from its dependency setup, which this
machine does not need. The x86 ILP32 job could not be run here.

One fresh local review compared all live bodies with the Mac disassembly and found no
behavioral difference. It noted that the first `jitter` control used a zero move and that
overflow did not assert the absence of a configstring write; both are now covered. Its
remaining limits stand: larger jitter steps and the half-width boundary are not exercised
by the control, and they rest on the full comparison.

Private evidence: the replay baseline in
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-missile-20261104.noindex/`.
