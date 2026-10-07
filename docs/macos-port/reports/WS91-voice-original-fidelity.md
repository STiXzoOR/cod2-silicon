# WS91: bounded original voice fidelity follow-up

Date: 2026-10-07. Base: `9a09b7b` (WS89 integrated).
Scope: the first diverging mode-5 enhanced subframe and mode-6 two-loss
concealment/recovery with enhancement disabled.

Two native-only repairs remove the diagnosed material differences: the comb
helper now uses the original weighted pitch-tap magnitude and floored RMS
normalization, and the decoder's negative third-tap history contribution reads
the third tap. No shared layout, mode table, quantizer, codec dependency,
voice wrapper or audio callback changes. The native-off operations and
third-party notices remain intact.

Mode-5 enhanced sequence RMSE against bounded original execution falls from
410.50 to 5.09 sample units; mode-6 two-loss sequence RMSE falls from 792.58 to
5.35. The mode-5 enhancement-off control remains 6.08. **This does not establish
original PCM parity.** Mode-2 and mode-8 enhanced sequences retain differences.

## Diagnostic evidence

The original oracle executes the same owned Mac 1.3 i386 binary and bounded
import models documented in [WS89](WS89-voice-compatibility.md). Synthetic
packets, initial state and random seed are identical. Private entry/return
hooks capture excitation, pitch taps, enhancement coefficients, comb memory
and synthesis-filter memory. Native helper wrappers use a fresh library
compiled from this worktree with the actual engine Release flags, including
`-ffp-contract=off`; no game or engine process runs.

In mode 5's first frame, decoded excitation agrees within 0.0011 sample units
and the comb helper receives identical pitch taps and comb gain. The first
comb output already differs by up to 0.8730; a later subframe differs by up
to 600.76. Its smoothing memory also differs. The original bandwidth helper
receives k3 = 0.12, while native derives 0.12048198; this separate caller
difference is retained. Original LPC arrays omit the native leading unit
coefficient, so array comparisons account for that representation difference.

Direct comb calls then use identical independently constructed synthetic
history, pitch taps, parameters and initial `CombFilterMem`. They establish
two differences inside original `comb_filter` at `0x1f19de`:

- Pitch magnitude is `abs(center) + positive outer taps + half the magnitude
  of negative outer taps`, averaged between current and previous tap sets.
  Native 1.0.4 instead takes the absolute value of their signed sum.
- Both input and enhanced energy become RMS with `sqrt(mean_square + 0.1)`.
  The normalization gain is the smaller RMS divided by `(output_RMS + 1)`,
  with the existing lower limit 0.5. Native instead uses a total-energy ratio.

The pitch threshold adjustments 1.3 and 0.5, interpolation, and smoothing
constants 0.96/0.04 already agree. Four direct synthetic helper cases initially
have maximum output errors from 0.153 to 8.49. The isolated corrected helper
agrees with original execution within 0.00000763; final smoothing memory agrees
exactly. Original code/data excerpts and decoded output remain outside Git.

In mode 6 with enhancement disabled, innovation agrees within roughly 0.0001
sample units before loss, and decoded three-tap gains agree. Before the first
loss, frame 2's original pitch average is 0.970703125, while native records
0.857421875. Native's negative third-tap branch subtracts half of the first
tap. Original decoder instructions at `0x1f5ef0..0x1f5f16` and the negative
branch at `0x1f64d9` use the third tap. The preceding three frame averages
produce different PLC medians: original 0.947265625, native 0.857421875.
This changes loss excitation and the pitch-gain limit on recovery.

Correcting that single accumulation with enhancement still disabled reduces
loss/recovery excitation RMSE from 65.92, 98.16 and 67.56 at frames 3–5 to
about 0.0002. The three-tap history-fill helper is unchanged. No difference in
gain-table storage domains is asserted as a defect.

## Independent regressions and acceptance

The new helper tests use constant histories, yielding constant raw comb
outputs 0.3, 37.5, 93.75 and 132.5. Independent expectations use the specified
RMS gain and the closed form `gain + (initial_gain - gain) * 0.96**sample`.
They do not call the engine to calculate an expectation. The four cases
detect normalization, floor, negative-tap magnitude and high-gain behavior.

Pitch-history literals are independently derived from the published Xiph
1.0.4 `gain_cdbk_nb`, using seven-bit indices at bit 47 + 81 × subframe in
the existing synthetic mode-6 packets. The first three index sets are
`[0, 41, 29, 61]`, `[95, 95, 59, 106]` and `[45, 29, 120, 127]`. Their frame
averages are 0.49609375, 0.947265625 and 0.970703125. These are synthetic
upstream-derived scalars; no original binary table or PCM payload is added.

Before source changes, all four comb tests and pitch frames 0/2 fail; pitch
frame 1 passes as a control. All seven pass after the repairs. The fixed
`fixtures.json`, independent regeneration driver and existing WS89 receipts
are unchanged. Its SHA-256 remains
`ff6001e5ee477ce0d3b99f31b17f75394f54aa2e6db7de5dc9a777a1290fed75`.

All 64 independently encoded 1.0.4 packets remain exact. All eight normal
enhancement-off PCM configurations and modes 1/8 loss configurations retain
the 0.25 maximum error limit: ten configurations, 80 frames. The 12 changed
enhancement/three-tap-loss comparisons retain their fixed 1.0.4 expected PCM
and appear as explicit non-acceptance observations. Their status/bitstream
checks still run; their tolerance is not widened and their expected PCM is
not replaced by modified native output.

The optional original oracle adds two bounded PCM acceptance checks for the
diagnosed configurations, requiring RMSE ≤ 15 and maximum error ≤ 64 plus
exact status/remaining-bit agreement. These limits catch the prior material
errors while allowing the smaller numerical/import-model differences already
seen in the enhancement-off control. They are not bit-exact PCM acceptance.

Final fresh verification, with the separate compatibility utility, reports:

| Check | Result |
| --- | --- |
| Stock actual Release flags plus original oracle | 57/57 pass |
| Stock ASan + UBSan | 55/55 pass |
| CoD2x ASan + UBSan | 55/55 pass |
| Independent 1.0.4 encoding | 64/64 packets and bit counts exact |
| Unchanged 1.0.4 PCM acceptance configurations | 80/80 frames within 0.25 |
| Independent pitch-literal provenance from pinned Xiph archive | 3/3 exact |
| Original observations, 22 configurations | Zero status/remaining-bit mismatches |
| Focused ABI, 17 TUs, stock and CoD2x | Zero errors, mismatches or new findings |
| Focused stub gate, same TUs, stock and CoD2x | Two existing candidates, zero new |
| Two production files, five native-off configurations | Zero mismatches |
| Two actual SDK preprocessing commands with native gate removed | Zero mismatches/errors |
| `git diff --check` | Pass |

The earlier red receipt reports six failures among seven new compatibility
checks, with all 48 WS89 checks passing. The final default run has 55 checks;
the optional original run has 57. No full engine integration or Linux multilib
object/binary byte comparison is claimed by this bounded unit. Native-off
preprocessing is supporting evidence, not a substitute for that byte gate.

## Remaining differences

Final original observations over the same eight-frame sequences are:

| Mode | Enhancement | Two losses | RMSE | Max absolute error |
| --- | ---: | ---: | ---: | ---: |
| 5 | 0 | No | 6.08 | 13.25 |
| 5 | 1 | No | 5.09 | 12.22 |
| 5 | 1 | Yes | 4.93 | 12.84 |
| 6 | 0 | Yes | 5.35 | 12.56 |
| 7 | 0 | Yes | 5.74 | 12.45 |
| 2 | 1 | No | 39.37 | 81.99 |
| 2 | 1 | Yes | 39.52 | 114.44 |
| 8 | 1 | No | 31.89 | 78.73 |
| 8 | 1 | Yes | 33.53 | 132.08 |

Mode 8's normal enhanced RMSE was 21.82 before this repair; restoring the
confirmed helper behavior increases that aggregate error to 31.89. This is
disclosed rather than hidden by a 1.0.4 tolerance change or parity claim.
The next bounded diagnostic is mode 2's first enhanced synthesis divergence:
compare caller bandwidth coefficients, then directly replay equal filter
inputs/state before proposing any coefficient or mode-table change. The
smaller enhancement-off numerical differences also remain unlocalized.

Private traces, direct-call observations, red/green reports and diagnostic
scripts are in
`/Users/stix/Projects/cod2-native-wt/_agents/research/voice/evidence.noindex/ws91`.
The final receipts are `final-stock-original.json`,
`final-stock-sanitized.json` and `final-codx-sanitized.json`; `red-stock.json`
retains the pre-repair failures. `contracts_provenance.py` reads only the pinned
official archive and committed synthetic packets to verify the pitch literals.
No capture, live audio, network, GUI, game, benchmark, global configuration,
system package installation or publication was performed.
