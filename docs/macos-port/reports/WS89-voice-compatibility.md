# WS89: bounded voice codec compatibility repair

Date: 2026-10-07. Base: `6e9a75f7d26537b414c0eab2cff7b1e528439c9e`.
Scope: WS44 FA08, narrowband Speex packet decoding and loss concealment.

Fixed independent packet vectors confirmed two empty native decoder loops:
three-tap pitch history was never populated, and lost-frame excitation was
never generated. Native-only repairs restore those operations. Actual Mac 1.3
codec execution also established three missing compatibility details: exhausted
frame headers return -1, float PCM output saturates at ±32767, and loss RMS
includes a 0.1 mean-square floor. These details are included in the repair.

The fixed synthetic corpus passes 48/48 checks in stock and CoD2x native builds.
This establishes the tested bitstream compatibility and independent 1.0.4 PCM
regression behavior. **It does not establish original Mac PCM parity.** Enhanced
output and some consecutive-loss/recovery sequences retain material differences.
FA08's broader original-fidelity question remains open.

No capture, audio callback, network wrapper, mode table, shared layout, platform
configuration or renderer code changes. Capture remains unavailable in the
existing native implementation. No game, GUI or benchmark was started. No
system/global package or configuration was changed. The source repairs are
independently authored, and existing third-party notices remain intact.

## References and reproducible inputs

The bundled codec identifies as `speex-1.0.4`, bitstream version 4. The principal
independent regression library is unmodified BSD-licensed
[Xiph Speex 1.0.4](https://downloads.xiph.org/releases/speex/speex-1.0.4.tar.gz),
archive SHA-256
`d0708ab9d1a6ae6aa8f59cf7b8c37c1cf03fc2c2fe20401a03b08c57c9898630`,
verified using [Xiph SHA256SUMS.txt](https://downloads.xiph.org/releases/speex/SHA256SUMS.txt).
It was compiled independently with `clang -arch arm64 -O3 -ffp-contract=off
-fno-strict-aliasing`, from its own headers and library sources. Its encoder
produced all 64 committed principal packets; its decoder produced all 176
committed expected PCM frames. The engine never generated those expected values.

The additional Speex 1.2.1 reference was the existing task-local GStreamer
x86_64 library, run under existing Rosetta. It reports bitstream version 4;
library SHA-256 is
`489035822ba8ab1484b6a144a99bdc84a7994a4ffe87baafdf8c14ecb77bd7f3`.
Its 64 synthetic packets check incoming compatibility, not original PCM parity.
No assumption about DXMT or a newer Speex release was used to establish the
original decoder's behavior.

The original-function oracle reads the owned, external Mac 1.3 i386 binary at
`/Users/stix/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
It executes actual decoder, bitstream and helper instructions under Unicorn
2.1.4. It uses the original mode data in memory. The task-local environment is
`/Users/stix/Projects/cod2-native-wt/_agents/research/voice/oracle-venv`; no package
was installed globally. Capstone 5.0.7 was used for private instruction review
and is not a fixture dependency.

The oracle models zeroing allocation, bounded arena/free behavior, overlap-safe
memory movement, current host libm and current host libc `rand` seeded 17.
Double-return imports use a checked cdecl/x87 thunk. Original RMS, scaling and
LSP interpolation plus the thunk and overlapping memory movement pass separate
self-checks. Unknown imports or exceeded instruction/time/heap bounds fail.
Diagnostic `fprintf` calls are suppressed and counted. Historical Darwin
libm/rand, allocator placement and a complete application environment are not
reproduced, so this is bounded function execution with explicit import models,
not a claim of full original-process or native i386 numerical equivalence.

See [tests/voice/README.md](../../../tests/voice/README.md) for commands, full
corpus provenance and regeneration. The fixed corpus needs no downloaded
dependency or original binary to run. Regeneration was byte-exact, SHA-256
`ff6001e5ee477ce0d3b99f31b17f75394f54aa2e6db7de5dc9a777a1290fed75`.

## Verified repair basis

All reference addresses are original i386 virtual addresses. The report retains
scalar facts and aggregate errors; original binary excerpts, tables and decoded
PCM stay outside Git.

### Three-tap history and lost excitation

Before the repair, `pitch_unquant_3tap` declared the per-tap history indices but
left each scratch buffer unfilled. Unmodified 1.0.4 and actual original decoding
both require those histories. Original `pitch_unquant_3tap` starts at
`0x1efda4`; its loop at `0x1efecb..0x1eff59` copies old excitation using
`pp = pitch + 1 - tap`, repeats one additional pitch period, then zero-fills
the rest. The native loop implements those index cases without modifying gain
tables or quantizers. The packet corpus covers modes 1 and 8 as controls and
the affected three-tap modes 2 through 7.

The lost-frame loop was also empty. Original execution at
`0x1f5b50..0x1f5bb4` combines pitch history and random innovation. The restored
native loop uses the existing pitch-gain/fade values and existing random helper;
it does not replace decoder state or alter the encoder. Two consecutive losses
and three recovery frames are included in every configuration's loss sequence.

The original loss path calls `compute_rms` (`0x1f123e`) at `0x1f5a65`. That
helper adds 0.1 after mean square and before square root; a direct helper call
on synthetic values 3, 4, 0, 0 agrees with float-rounded `sqrt(6.25 + 0.1)`.
An initially lost frame produces small nonzero original noise. Native baseline
output was effectively zero; the restored loop and floor reproduce that
contract. The floor is established from the actual Mac helper, rather than
assumed from the older upstream decoder, which lacks it.

### Exhausted headers and return codes

Original `nb_decode` (`0x1f52f6`) checks available bits before consuming the
layer flag (`0x1f5653..0x1f565b`), after skipping a first wideband layer
(`0x1f56cf..0x1f56dd`), and before reading the narrowband mode
(`0x1f5704..0x1f570c`). These are the three corresponding native guards.
An empty packet and the exhausted layer header `80` return -1 in the original
oracle; both incorrectly returned 0 before the repair. Terminator `7f` returns
-1; invalid narrowband `48` and invalid wideband `e0` return -2.

The one-byte mode-5 packet `29` contains a complete header followed by truncated
payload. Original execution returns 0 with remaining bits -1. The repair keeps
that behavior and does not turn this bounded header check into a new malformed
payload rejection policy. Both normal return codes and bit counts are checked.

### Output saturation and state order

Original ordinary output is copied with ±32767 saturation at
`0x1f5556..0x1f55b2`; the loss path has a separate equivalent output copy at
`0x1f5be9..0x1f5c37`. Saturating input tests cover ordinary, lost and null-mode
returns. The synthetic stress packet is an independently encoded mode-5 packet
with global-gain bits set to their maximum, not an original game packet.
Baseline native ordinary output reached approximately -45076 to 53660, lost
output -45285 to 72791, and null-mode output -34757 to 19293. The original and
repaired native returns respect ±32767 in each case.

The new helper clips the caller's output only. Native `pre_mem` is updated from
unclipped output before clipping, as it was before this patch; frame, excitation
and synthesis-filter buffers are not clipped. The native narrowband mode's
`preemph` is 0, so `pre_mem` cannot affect the audited narrowband output. The
original `DecState` has no `preemph` or `pre_mem` fields, so no original
`pre_mem` ordering is asserted. The original loss path clips before its
first/count-loss/pitch-ring updates (`0x1f5c37..0x1f5c5b`), whereas the native
helper clips after those existing updates. None reads the caller's output, so
that ordering does not change state. Existing native state-update order is
preserved; no state or layout is imported from the original binary.

## Results and remaining original differences

The first reference-first baseline failed 23 of 46 checks before any repair:
missing adaptive excitation/loss PCM, exhausted headers, saturation and initial
comfort noise. The two additional branch-specific clipping checks were added
after direct baseline/original characterization. Fresh final runs have 48 checks.

| Verification | Result |
| --- | --- |
| Stock actual Release flags, with optional original oracle | 48/48 pass |
| Stock actual Release flags, ASan + UBSan | 48/48 pass |
| CoD2x actual Release flags, ASan + UBSan | 48/48 pass |
| Independent 1.0.4 packet equality | 64/64 byte- and bit-count exact |
| Independent 1.0.4 PCM, 176 frames | Max absolute error 0.0215; limit 0.25 |
| Incoming independent 1.2.1 packets | 64/64 accepted, finite output |
| Original oracle, 22 normal/loss configurations | Zero status/remaining-bit mismatches |
| Focused ABI audit, 17 codec/wrapper TUs, stock and CoD2x | Zero errors or mismatches |
| Focused stub gate, same TUs, stock and CoD2x | Two existing candidates, zero new |
| `tools/abi/legacy.py`, five native-off configurations | Zero mismatches |
| Exact SDK preprocessing, two changed TUs with native gate removed | Zero mismatches/errors |
| `tests/fixes13/legacy.py`, ten configurations | Zero mismatches |

The CoD2x-only inactive-gate checker was also run and reports 14 mismatches
across the two files. Its policy requires changes to disappear with either
CoD2x gate inactive and tests `COD2_X64=0` as a defined macro. These repairs
intentionally apply to stock native builds; they are not CoD2x hooks.
`CONTRIBUTING.md` explicitly requires CMake native OFF to omit `COD2_X64`,
because existing code uses `defined(COD2_X64)`. That checker is not applicable
to this repair's scope; the dedicated native-off checks above pass. No full
Linux multilib object/binary comparison or full engine integration gate was
performed in this bounded codec worktree; preprocessing alone does not prove
legacy object-byte equality.

Original PCM error was sharply reduced but remains real. Ordinary,
non-enhanced mode 1 through 8 sequences have RMSE 5.56–7.18 sample units. The
worst pre-repair original comparison had RMSE 7239.47. Selected current
original comparisons over the entire eight-frame sequence are:

| Mode | Enhancement | Two losses | RMSE | Max absolute error |
| --- | ---: | ---: | ---: | ---: |
| 2 | 1 | No | 616.85 | 2978.52 |
| 5 | 1 | No | 410.50 | 1398.60 |
| 5 | 1 | Yes | 252.19 | 1296.11 |
| 6 | 0 | Yes | 792.58 | 2764.83 |
| 7 | 0 | Yes | 643.80 | 1817.34 |
| 8 | 1 | No | 21.82 | 56.72 |

These are not justified as harmless rounding or attributed to a particular
codec release. The original STABS `SpeexSubmode` has a third enhancement
coefficient absent from the native 1.0.4 structure, while native code derives
one from two coefficients. That is a concrete structural difference, not yet
a causal demonstration for the PCM errors. The larger loss-sequence errors
also occur with enhancement off, so enhancement alone cannot explain them.
Neither a different helper name nor linear-versus-log excitation table storage
establishes a defect: the latter represents different gain domains, as WS44
already warned. Those tables remain unchanged.

The narrowest next diagnostic is the first diverging subframe of mode 5 with
enhancement on, using the same fixed packet with enhancement-off as its control:
compare decoded excitation, enhancement coefficients, `comb_filter` input/output
and synthesis memories at the original/native call boundaries. Then call each
suspect helper directly on the same captured synthetic inputs and initial state.
This separates different original caller parameters from another incomplete
native helper without changing tables or claiming a release-version cause.
For the independent loss issue, mode 6 with enhancement off and frames 3–5
isolates innovation RMS, PLC excitation and the recovery pitch-gain limit with
the already verified history-fill loop as a control. A new behavioral repair
needs those function-level observations before widening this patch.

Private reports, archive/source builds and observations are in
`/Users/stix/Projects/cod2-native-wt/_agents/research/voice/evidence.noindex`.
The final report files are `repaired-stock.json`,
`repaired-stock-sanitized.json` and `repaired-codx-sanitized.json`; original
output stays private and only aggregate errors appear above. The public fixture
and generic optional oracle contain no original decoded PCM or table bytes.
