# WS50 — Sound environment fidelity

2026-10-07; base `92dd61e` (WS48 atop the 0.2.1 integration baseline).

## Changes and reference evidence

FA06: native 2D and 3D reverb now select the environment wet level when there
is no alias, or when reverb is enabled and alias flag `0x10` is clear. All
other cases select zero. Original Mac branches at `0x56897`–`0x568d8` and
`0x56919`–`0x56952` establish that selection. Both native implementations
previously inverted it. Original i386 source remains unchanged after removing
the native preprocessor branches.

FA07: `snd_setEnvironmentEffects` now validates its six arguments and calls
the existing sound-state implementation. The native empty stub is removed;
the implementation lives beside the related deactivate command in `snd.c`.
Reference function `0x4cf46` validates priority and room before dry/wet levels
and fade time. Float arguments retain `atof` semantics, levels are checked
against 0–1 and negative fade milliseconds are rejected. Diagnostic wording
is independently authored; byte-identical console strings are not claimed.

The original priority loop starts at **one**, accepts `level` and
`shellshock`, and stops before three (`0x4cf90`–`0x4cfb3`). The WS44 prose
also mentioned `none`, but the actual command does not accept priority zero.
The regression therefore rejects `none`, `effect` and unknown priorities.

The registered command already points to this function in `SND_Init`; no
registration or dispatch API changes were needed. Native AIL wet-level setters
in `macos_audio.c` write the sample wet field, which feeds the mixer wet buffer
at lines 386–387 of this baseline. This confirms a real audio consumer, but no
acoustic capture was performed by this unit.

## Verification

- Existing independent reverb probe: **10/10 differences before**, **0/10
  after**, in both stock and CoD2x modes. No expected-difference option used.
- New command regression failed against the active empty stub at the first
  expected effect-priority change. It now passes with ASan/UBSan in both modes,
  exercising real room lookup and effect state/goals/fade rates, case-insensitive
  valid arguments, wrong argument counts, invalid priorities/room, out-of-range
  levels, negative fade, zero fade and uninitialized sound behavior.
- All three changed production translation units compile with actual stock
  and CoD2x Release commands (six compilations). This is focused compilation,
  not a new complete linked build.
- Three changed source files across ten legacy preprocessing configurations:
  zero differences. `unifdef -UCOD2_X64` output is also byte-identical to the
  baseline for all three files. Linux multilib objects were not built.
- Scoped source stub gates: zero new candidates in both modes. The old empty
  environment-command candidate disappears without altering the baseline.
- Fidelity tool unit suite remains green. Both audio contract commands are
  now included in CONTRIBUTING's merge gate.

The first new command fixture needed `<stdlib.h>` for the extracted body's
`atof`/`atoi` declarations; its intermediate failed run is retained. The
production source already declares those functions correctly.

Evidence: private
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/`,
with `reverb-*-red/green`, `environment-*`, `audio-*-stubs` and compile logs.
Whole-game acoustics, voice codec vectors and audible footsteps remain separate
acceptance work. This unit did not run a game or change the audio backend.
