# F1 — dead player movement state, 2026-10-26

`ClientEndFrame` now selects ordinary dead movement (`pm_type = 6`) when
`tagInfo` is zero and linked dead movement (`7`) when it is nonzero. The native
code previously selected between them using `count == 1`. The correction is
one assignment behind `COD2_X64`; the original-build assignment is preserved.
`player_die` is unchanged: its existing 0/1 to 6/7 transition was already
consistent with the Mac reference.

## Reference evidence

The authority is the owned Mac MP 1.3 i386 binary. The bounded dead branch of
`ClientEndFrame` starts at `0x1a4f3f`: the comparison at `0x1a4f42` reads entity
field `+0x208`, followed by unsigned-borrow arithmetic that produces 6 for
zero and 7 for every nonzero value. A fresh STABS read identifies `tagInfo` at
520 bytes (`0x208`) and `count` at 416 (`0x1a0`). Native `tagInfo` is a four-byte
encoded handle; testing nonzero needs no pointer decoding.

An independent Mach-O segment-mapped comparison checked the discovery
instructions against the actual binary: 459 instructions, 2,032 bytes, zero
mismatches for `ClientEndFrame`. This establishes the provenance of the
instructions examined, not whole-function semantic equivalence. The same
branch retains the existing dead-state `svFlags` changes and clears
`takedamage`; noclip and ufo retain their existing higher priority.

Private evidence is under
`~/Library/Application Support/CoD2-native-decomp/F1-dead-state.noindex/`.
Original instructions and binaries are not included in this repository.

## Proof and verification

`tests/lp64/game/client_end_frame.py` builds the complete production
`g_active_mp.c` translation unit with the selected engine compile database,
assertions enabled, ASan and UBSan, and `-ffp-contract=off` inherited from the
engine flags. The separate synthetic C fixture controls external model,
entity-conversion, view and animation services. It exercises production
`ClientEndFrame`, `G_SetClientContents`, `P_DamageFeedback` and the applicable
`StuckInClient` path, then checks stored player movement state and server
flags. External services outside the selected paths abort if called.

Both stock and CoD2x historical runs at `8cec375` produced exactly three
failures and six passing controls. The failures were null `tagInfo` with count
0, nonzero `tagInfo` with count 1, and a high-bit nonzero handle with count 1.
Passing controls covered the other null/nonzero-count combinations, noclip
over ufo/dead, ufo over dead, and alive null/nonzero states. The fixture and
runner were then frozen before the production edit.

After the correction all nine cases passed in each build configuration, with
no sanitizer errors. Every case checks both the movement state and
`svFlags`/`takedamage`, plus completion of end-frame button clearing. Synthetic
nonzero handles are deliberately not dereferenced. The tests use no licensed
assets and make no network requests.

Commands, with fresh external output directories:

```sh
base=/Users/stix/.codex/worktrees/metal-u1-integration/cod2-native
out="$HOME/Library/Application Support/CoD2-native-decomp/F1-dead-state.noindex"
for mode in stock codx; do
    build=build-macos
    if [ "$mode" = codx ]; then build=build-macos-codx; fi
    taskpolicy -b nice -n 19 python3 tests/lp64/game/client_end_frame.py \
        --compile-db "$base/$build/compile_commands.engine.json" \
        --output "$out/final-$mode"
done
```

The `red-stock` and `red-codx` runs used the same command before the edit.
`--baseline 8cec375` permits reproducing those historical failures after it.
The initial `baseline-stock` attempt stopped at link time while the fixture's
external service declarations were being completed; it is not counted as a
behavioral red run.

`unifdef -UCOD2_X64` output for the production file exactly matches `8cec375`
by `cmp`: 45,738 bytes. All five inactive legacy guard configurations matched,
and `python3 tests/fixes13/legacy.py --base 8cec375` reported one changed source
file, ten configurations, zero mismatches. `git diff --check` passed. These
source checks do not substitute for the unavailable Linux i386 object/binary
comparison.

## Limits and integration

The saved replay diagnosis placed the first death-state difference at frame
499 (original 6, native 7), followed by a movement difference at frame 500.
Those traces did not include `tagInfo`, count or all branch-control inputs.
The bounded source repair is proven; this worker has not rerun the replay and
makes no claim that it resolves that divergence or later differences.

Rendering, full animation/script execution, spectator/intermission paths,
model changes, damage feedback with accumulated damage, smooth extrapolation
and collision ejection are outside this fixture's scope. No game or VM was
started. The orchestrator owns CI registration, integrated builds, replay,
independent simplification/review and any commit or push. This worker leaves
four changed paths on `decomp/f1-dead-state` without commits.

## Integrated verification

The canonical integration repeated both nine-case sanitizer fixtures, linked
stock and CoD2x client and dedicated-server targets, and passed the scoped
legacy, fixes13 and game legacy checks against `8cec375`. Function ABI audits
covered 634 stock and 651 CoD2x translation units with zero errors or
mismatches. The fixture is registered in CI for both configurations.

A fresh stock replay used the frozen two-client command corpus for 2,000
frames (4–2003). Against the original trace, every recorded `pm_type` now
matches, as do active-entity counts, health, weapon and weapon state. Compared
with the prior native trace, the first changed player value is frame 499's
linked-dead state 7 becoming ordinary dead state 6. At frame 500 the corpse
retains movement instead of incorrectly zeroing its velocity. The correction
changes 41 player movement-state records and 40 position/velocity records.

The unchanged complete comparator still fails: the first player difference
is a one-bit velocity difference at frame 102, and the first quantized state
hash difference remains frame 116. Later first differences remain movement
flags at 652, view angles at 776 and movement timer at 1803. This replay
confirms the bounded dead-state repair, not complete server parity.

A second corrected stock replay matches all 2,000 frames under the unchanged
quantized-state and raw-player-field comparator. CoD2x also completes the
2,000-frame corpus with both clients and no script-runtime-error marker.
This CoD2x completion is a smoke result, not a stock-reference parity claim.

Root evidence and executable commands are under
`~/Library/Application Support/CoD2-native-decomp/F1-dead-state-integration.noindex/`
(`verify.py`, `abi.py`, `replay.py`, `compare-results.py` and their outputs).

Independent focused review of all five paths found no actionable issues.
The separate Claude peer confirmed model `claude-opus-5-5`; high effort was
requested but actual effort was unverified. All five simplification lenses
required no edits; frozen proof-fixture comments were retained. The review
receipt is machine-local at
`/tmp/compound-engineering-501/ce-code-review/20261026-134844-539a9f37/review.json`.
The synthetic fixture omits the full death lifecycle; the integrated replay
exercises the observed death transition with actual game services.
