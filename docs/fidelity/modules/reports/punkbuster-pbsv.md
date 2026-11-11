# punkbuster/pbsv reconciliation

Updated 2026-11-05.

38/38 reconciled: 37 existing intentional rows retained exactly, 1 fixed engine-helper row. The prior bundled PunkBuster verdicts are unchanged: they make no semantic-equivalence claim, and native uses the documented null adapter in src/PC/punkbuster/pb_null_adapter.c rather than loading the discarded upstream backend.

The extra itoa entry at Mac 0x25994 is an i5stdlib engine helper, not part of that exemption. Mac negates every negative input regardless of radix, extracts signed remainders/quotients, emits uppercase digits and an optional minus, then reverses strlen bytes. Native previously applied the minus only in radix 10 and treated other negatives as unsigned. It also evaluated signed -INT_MIN. Native now follows the all-radix sign rule, uses explicit 32-bit wrapped negation, and retains the Mac signed-remainder behavior for INT_MIN. Output is written directly before reversing, preserving the original C-string reversal contract.

The arm64 Release dedicated target compiled and linked this source. The correction was reviewed against 0x25994-0x25a58 but not exercised. OFF preprocessing matches the base and there are no __LINE__ expansions. No new tests, full CI or replay ran. No bundled library was reconstructed.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
