# Native audio output contract

Run this fixture in stock and CoD2x modes using the actual engine compile
databases. It remaps their source/include paths into the current checkout and
builds only private fixture objects; it does not change engine objects.

```sh
taskpolicy -b nice -n 19 python3 tests/audio/output.py \
  --compile-db build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-audio/stock"
taskpolicy -b nice -n 19 python3 tests/audio/output.py \
  --compile-db build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-audio/codx"
```

The fixture compiles the production sound positioning, start/update, listener,
channel-offset and entity-sound bodies. It links the real `CG_EntityEvent`,
math functions and native audio implementation with ASan/UBSan and assertions.
The test-only render bridge invokes the same static `Render` callback that the
AudioUnit uses, with a stereo float output buffer. No audio device is opened.

Coverage:

- Eighteen front/right/left/back/up/down cases under translated, yawed and
  pitched/rolled listeners check Miles coordinates and emitted stereo PCM
  through both `MacAudio_Mix` and `Render`.
- An entity translated and yawed after recording a nonzero local offset checks
  the real channel-offset producer, update consumer and rendered PCM.
- Sample start checks a 32 ms delay, 48 kHz input with 96 kHz playback,
  stop/resume cursor preservation and exactly 240 output frames from a
  480-sample input.
- All 138 run/walk/prone first/third-person surface events check the emitted
  alias-list slot and stereo PCM against an independent PCM16 decoder and
  linear resampling expectation.

The deterministic picker uses one alias per list. Channel allocation, alias
randomization and equipment audio are fixture boundaries; equipment emissions
are checked, but their synthetic alias has no sound file. This is a bounded
producer-to-output contract, not a complete game session or original Miles
acoustics comparison.

An optional private mono PCM16 WAV can replace the synthetic footstep input:

```sh
taskpolicy -b nice -n 19 python3 tests/audio/output.py \
  --compile-db build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-audio/owned-footstep" \
  --case footsteps --wave /absolute/private/path/footstep.wav
```

Keep licensed assets and all resulting evidence outside the repository. Output
PCM and the render callback are tested in memory. These checks do not prove
that a speaker or headphone emitted sound, or establish complete reverb,
occlusion, surround or hardware latency fidelity.
