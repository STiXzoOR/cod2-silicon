# WS90 — Native positional sample output fidelity

2026-10-07. Base `69f2238`, branch `codex/audio-output-fidelity`. The initial
tree was clean. Only `src/PC/win32/snd_driver.c`, three new audio fixture files
and their documentation are changed. No audio backend or voice code change
was necessary.

## Reference findings and changes

Original Mac 1.3 `SND_Set3DPosition` at `0x5696c` subtracts the listener origin
and passes these coordinates to Miles at `0x56a4d`:

```text
right   = -dot(delta, listener.axis[1])
up      =  dot(delta, listener.axis[2])
forward =  dot(delta, listener.axis[0])
```

The listener producer copies the cgame view basis (`SND_SetListener`,
`0x4aa14`); production `AnglesToAxis` establishes forward/left/up ordering.
The old native sample call passed forward/right/up instead. The native mixer
uses Miles X divided by distance as its left/right pan, so a sample directly
ahead was incorrectly rendered in the right channel. The fix reorders only
the native call's arguments. Stream start (`0x57c52`) and update (`0x56be0`)
already perform the correct reorder in the reconstructed source; their
misleading local variable names did not justify another change.

Original `SND_Update3DChannel` at `0x58318` passes
`g_snd + 0x370 + index * 0x50` to the current-position helper (`0x5838b` to
`0x58398`). STABS identifies `snd_local_t.chaninfo` at `0x334`, channel size
`0x50`, `offset` at channel `+0x3c` and `system` at `+0x4c`. The argument is
therefore the three-float `offset`. The native source instead passed the
address of `system`, losing the entity-local sound displacement and reading
neighboring fields as floats. The native update now passes `ci->offset`.

Both fixes use `COD2_X64` gates and retain the original i386 expressions.
The changes were independently authored from factual reference observations;
no reference implementation or disassembly is stored in this report.

## Reproduction and verification

The new [output fixture](../../../tests/audio/OUTPUT.md) compiles production
function bodies and the native mixer/render callback. Independent expected
coordinates and PCM values were written before production edits.

- Baseline: directly-ahead start emitted PCM `(0, 0.1125)` instead of the
  independently expected centered `(0.0795495, 0.0795495)`. The footstep output
  assertion also failed on asymmetric PCM.
- Basis-only fix: all 18 direction cases, start/rate/duration checks and 138
  footstep cases passed, while the moving-offset case independently remained
  red (`right = -0`, expected `25`).
- Final fixes: stock and CoD2x pass all four cases with ASan/UBSan. Direction
  cases cover translated, yawed and pitched/rolled listeners through both
  `MacAudio_Mix` and the actual `Render` callback. Moving-offset output is
  checked through `Render`, including the game's linear falloff curve. Review
  strengthened this case by translating and yawing the emitter after recording
  its offset; the independent final Miles coordinate is `(-10, -8, 75)`.
- Start checks cover 48 kHz PCM with 96 kHz playback, a 32 ms delay, pause via
  stop/resume without resetting the cursor and exactly 240 output frames from
  480 input samples. This is emitted-frame timing, not a hardware latency test.
- The real `CG_EntityEvent` and `CG_PlayEntitySoundAlias` emit all 138
  run/walk/prone first/third-person surface slots. A deterministic one-entry
  picker and direct start adapter connect these to the production sample start
  and renderer. Equipment slot emissions are verified with silent synthetic
  equipment aliases. The test does not exercise randomization, channel
  competition or full `SND_PlaySoundAlias` dispatch.
- A privately extracted owned dirt-footstep WAV (44.1 kHz, mono PCM16,
  20,832 samples) passes the same 138 slot-to-render cases in both modes.
  Its first eight stereo output frames match an independent PCM16 decode and
  linear resampling expectation with centered pan and game attenuation.
- The changed production translation unit compiles with actual stock and
  CoD2x Release flags, remapped into this tree, into private objects. Existing
  environment command/reverb checks pass in both modes, including 10 reference
  reverb cases with zero differences.
- Ten legacy preprocessing configurations report zero differences.
  `unifdef -UCOD2_X64` produces byte-identical source to the base. This is guard
  evidence; Linux multilib object/binary equivalence was not run here.

All compilation ran with `taskpolicy -b nice -n 19`, at most two fixture
compilers concurrently. Parent engine objects were neither used nor modified.
No game process was launched, and no display/benchmark slot was acquired.

Private evidence is under
`~/Library/Application Support/CoD2-native-ws90/`: baseline red,
basis-only intermediate, stock/CoD2x green, owned-footstep, Release compilation
and environment logs. The owned WAV remains private and the licensed install
was read only.

## Limits and handoff

The fixture verifies emitted aliases and actual output-buffer PCM. It does
not prove physical speaker/headphone sound. No original Miles runtime,
same-input acoustic comparison, live gameplay, reverb/occlusion/surround
parity or device latency capture was performed. No backend rewrite is implied
by these bounded findings. Integration still needs the maintainer's full gate
and live acceptance on the final linked binaries.

The production fix is nine added lines and was manually reviewed for native
guard preservation and stream-path consistency. A broader simplification pass
was skipped because it would add scope to two narrow repairs. Changes are
unstaged and uncommitted for orchestrator review; no push occurred.
