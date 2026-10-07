# WS93 — scoreboard border rectangles

The native scoreboard now allocates all six border records and draws each
record's x, y, width, and height in order. The reconstructed consumer reserved
four records even though `CalcBorderLines` writes six, and started its float
cursor one element late. The two errors caused a stack overwrite and large
translucent rectangles when the scoreboard was displayed. This is a bounded
scoreboard repair; the separately observed yellow-green world tint is tracked
by WS94.

Worktree: `fidelity-followup/cod2-native`, branch
`codex/scoreboard-rectangles`, base `f5b83ad`. The frozen integration worktree
was not edited.

## Cause and original reference

`CG_DrawScoreboard`'s old consumer reads each border as y, width, height, and
the next border's x. For example, the existing left border `(123, 27, 2, 420)`
becomes `(27, 2, 420, 515)`, producing a strip from x=27 to x=447 that reaches
past the bottom of a 640×480 screen. Its last iteration also reads past the
six-record buffer. The supplied before/during/after screenshots show these
strips only while the scoreboard is visible, matching the bad draw geometry.

The owned Mac 1.3 binary was inspected read-only. `CalcBorderLines` at
0x1c50a8 writes six 16-byte records and returns six at 0x1c5142. The consumer
passes its record buffer at 0x1c6f64; the draw loop at 0x1c7662 points to the
first record's height, reads x/y/width/height in order, and advances 16 bytes
at 0x1c76a8. Its stack region accommodates the six records before that region
is reused for the map-name buffer. The native repair follows these contracts
with typed field access. No binary bytes, original disassembly, licensed
shader data, or screenshots were added to the repository.

## Validation

The new `tests/hud/scoreboard.py` fixture extracts the actual active production
buffer declaration, producer function, and border draw block. Assertions require
six positive, finite, thin rectangles inside the existing scoreboard panel,
four horizontal and two vertical borders, and preserved alpha across three
draws. Expected bounds and line counts do not reuse producer coordinates.

- Base `f5b83ad`: ASan reports a stack-buffer-overflow in `CalcBorderLines`.
- Capacity-only intermediate: the emitted rectangle fails the panel-bounds
  assertion, independently detecting the shifted consumer.
- Final stock and CoD2x fixtures: pass under ASan/UBSan.
- Existing turret/spectator HUD contracts: 8/8 pass for stock and 8/8 for CoD2x.
- The complete production scoreboard translation unit compiles using the
  respective stock and CoD2x engine flags.
- Actual engine-flag preprocessing with `COD2_X64` omitted is identical to
  `f5b83ad` for stock and CoD2x. This proves guard preservation for those
  configurations, not legacy object or binary equivalence.
- The production source body is unchanged in all five original-OFF
  configurations from the legacy guard checker.
- `git diff --check`: pass.

The parent independently reviewed the patch and reran the stock sanitizer
fixture. A private one-translation-unit stock overlay then received real SDL
Tab press/release events through the production input pump. Before/during/after
engine captures and a repeated toggle complete with exit zero: the corrected
scoreboard has thin borders within its panel and the large translucent strips
are absent. The unchanged yellow tint remains visible in this scoreboard-only
overlay, separating the two defects. Original history search places both bad
scoreboard expressions at initial commit `63de35a`.

Evidence remains under the private
`CoD2-native-current-readiness/ws93-render-state.noindex` directory: baseline,
capacity-only, and green scoreboard fixture directories, production compile
logs, HUD regression results, and the parent's screenshot captures.

All production changes are inside `COD2_X64` guards. Shared layouts, shader
state, existing border values, background geometry, and old OFF code are
unchanged. The fixture does not execute the full scoreboard, font rendering,
or physical input path; the parent owns independent review and live Tab
retesting. No runtime or complete rendering parity claim is made here.
