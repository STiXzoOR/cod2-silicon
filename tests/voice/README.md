# Narrowband codec fixtures

These tests compile the engine's 15 Speex translation units with the actual
Release engine flags. They exercise synthetic narrowband packets and decoded
float PCM without starting the game, opening an audio device, or using game
data. The platform voice test covers the native audio callback; this fixture
covers the codec that callback test does not call.

From the repository root on arm64 macOS:

```sh
python3 tests/voice/run.py \
  --compile-db build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/voice-stock.json"
python3 tests/voice/run.py \
  --compile-db build-macos-codx/compile_commands.engine.json \
  --sanitizers \
  --output "$HOME/Library/Application Support/CoD2-native/voice-codx-sanitized.json"
```

The default regression needs Python 3.9+, the existing compiler/SDK, and an
engine compile database. It downloads and installs nothing. It remaps another
checkout's compile paths to this checkout, compiles temporary objects, and
deletes them on exit. Compilation runs under `taskpolicy -b nice -n 19`.
`--sanitizers` adds AddressSanitizer and UndefinedBehaviorSanitizer. Reports must
stay outside the repository, including reports made without the private oracle.

## Playback without microphone capture

```sh
taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py \
  --compile-db build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/voice-init-stock"
```

Repeat with the CoD2x compile database and a fresh output directory. Add
`--baseline 7dac4d2` to reproduce the initialization failure. The fixture calls
the production `Voice_Init`, native unavailable recorder, `CL_VoicePacket`,
message readers, encode/decode wrappers, Speex, playback queues and native
`Render` callback. It never explicitly initializes DSound. A fixed independent
mode-5 synthetic packet must produce finite, nonzero mixer output for all 64
talkers. It checks mute/`cl_voice` gates, the 299 ms talk indicator boundary,
`+talk`/`-talk`, silent local input, microphone mixer call exclusion, two full
lifecycles, late packets, output-device failure and the one-time stderr notice.
It also verifies that decode converts only the 160 produced samples and returns
320 PCM bytes. ASan/UBSan stay enabled throughout.

The output-device creation boundary is controlled, so no audio device opens.
The successful-recorder control checks capture and mixer call counts using a
synthetic recorder; it does not implement or test real microphone capture.
Dvar registration, command arguments and player mute lookup are fixture
boundaries. The receive entry is `CL_VoicePacket`, after connectionless command
dispatch; no UDP socket, live server, game, hardware sound or original PCM
parity is claimed. Temporary build files are removed; logs and a JSON receipt
remain in the private output directory.

## Playback rate, duration and pitch

```sh
taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py --rate-contract \
  --compile-db build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/voice-rate-stock"
```

Repeat with the CoD2x database. `--baseline 26cf9bb` reproduces the 11025 Hz
failure. The same production receive/decode/queue/render path consumes 64 fixed
packets independently encoded by unmodified Xiph Speex 1.0.4 from a synthetic
512 Hz integer sine at 8192 Hz. The 10240 decoded samples must occupy 60000
48 kHz output frames (1.25 s), within one output frame; a windowed spectral
peak search must find 512 Hz within 0.25 Hz. Zero and negative recording-rate
globals must retain the same playback semantics through the fallback.

The separate rate fixture reuses the initialization fixture's controlled
capture boundary and calls the real digital-driver function with a test-owned
existing-output-unit sentinel, retaining format validation and the shared
default-rate update while skipping hardware creation. Native sample type,
playback-rate setting, PCM conversion, buffer queues, linear resampling and
`Render` are real. An unrelated
48 kHz sample must retain exactly 480 frames of constant stereo PCM before
voice startup, after startup and after shutdown; a newly allocated game sample
is also checked after startup. No audio device, game or UDP socket opens.
These checks establish rendered timing/pitch and that bounded
non-voice control; they do not establish original PCM, resampler or hardware
audio equivalence. The original Mac uses an AudioConverter for mixer-rate
conversion, while the native mixer uses linear interpolation.

The fixed packets need no external dependency to run. To regenerate them,
supply the same SHA-256-pinned official archive used by the codec corpus:

```sh
taskpolicy -b nice -n 19 python3 tests/voice/tone_packets.py \
  --source-archive /absolute/private/path/speex-1.0.4.tar.gz \
  --output /absolute/private/path/tone-packets.json
cmp tests/voice/tone_packets.json /absolute/private/path/tone-packets.json
```

## Independent expected values

`codec_probe.c` generates eight 160-sample input frames from an integer triangle,
a short saw, and unsigned 32-bit LCG noise initialized to 1. The driver calls
`srand(17)` before each encoder/decoder process. It uses the same public codec
API for separately loaded libraries. No engine implementation computes the
expected packet or PCM values in `fixtures.json`.

The principal reference is an unmodified build of the BSD-licensed official
[Xiph Speex 1.0.4 archive](https://downloads.xiph.org/releases/speex/speex-1.0.4.tar.gz).
Its SHA-256, checked against
[Xiph's checksum file](https://downloads.xiph.org/releases/speex/SHA256SUMS.txt), is
`d0708ab9d1a6ae6aa8f59cf7b8c37c1cf03fc2c2fe20401a03b08c57c9898630`.
Both that library and the bundled engine identify as `speex-1.0.4`, bitstream
version 4. The fixture contains:

- 64 independently encoded packets: eight frames in each of modes 1 through 8.
- 176 independently decoded frames: all eight modes with enhancement disabled,
  plus modes 2, 5 and 8 with enhancement enabled. Each configuration has a normal
  sequence and a sequence with two consecutive losses at indices 3 and 4,
  followed by recovery at indices 5 through 7.
- 64 independently encoded Speex 1.2.1 packets from the existing task-local
  GStreamer runtime, used to check incoming bitstream acceptance and finite
  output. Its library hash is recorded in `modern_provenance`; its PCM is not
  an original-game parity oracle.
- Header exhaustion/error cases and a synthetic saturation packet. The latter
  starts with the first mode-5 reference packet and sets global-gain bit positions
  35 through 39 to 1. Repeating it tests ordinary output, then following it with
  a lost frame or null-mode packet tests both other clipping paths.

The 55 checks require exact encoded packet bytes and bit counts, matching
decode return values and remaining-bit counts, and a maximum absolute PCM
error of 0.25 relative to independent 1.0.4 for all normal sequences with
enhancement disabled and loss sequences in modes 1 and 8. This PCM tolerance
permits scalar floating-point differences, including the verified Mac
comfort-noise floor; it is not a claim of bit-exact PCM. The initial-loss noise check and header/
saturation contracts were separately checked against the actual Mac 1.3 decoder.
The one-byte truncated mode-5 payload deliberately retains the original return
value 0 with `speex_bits_remaining == -1`; exhaustion before a complete frame
header returns -1. This is a compatibility fixture, not a general packet-fuzzing
or malformed-payload hardening change.

Original comb enhancement and three-tap loss history intentionally differ from
unmodified 1.0.4. The fixed corpus is unchanged: the remaining 1.0.4 PCM
comparisons are retained in `upstream_1_0_4_pcm` as observations with
`acceptance: false`. Their packet/status checks still run. PCM tolerance is
unchanged for the ten unaffected configurations, covering 80 frames.

Seven additional checks exercise the repaired behavior independently.
`compatibility_probe.c` calls the real comb helper with constant synthetic
excitation/history and known pitch taps. `contracts.py` evaluates the constant
raw output and smoothing through a closed-form expression, without using the
engine to generate an expectation. Four cases cover the RMS floor, the RMS
normalization denominator, negative outer taps and high-gain adjustment. The
helper output limit is 0.0003 sample units. Three pitch-history checks use the
first three fixed mode-6 packets: their seven-bit gain indices and tap values
were independently read from the published Xiph 1.0.4 `gain_cdbk_nb`, giving
frame averages 0.49609375, 0.947265625 and 0.970703125. These synthetic scalar
expectations contain no original binary tables or decoded original PCM.

The principal `codec_probe.c` driver and regeneration path remain unchanged.
See [the WS91 report](../../docs/macos-port/reports/WS91-voice-original-fidelity.md)
for the direct-call evidence, failing baseline and remaining differences.

## Regenerate the synthetic corpus

Regeneration is separate from running the tests. Supply the official archive
and the audited existing x86_64 Speex 1.2.1 library. `regenerate.py` pins both
hashes, builds unmodified 1.0.4 in a temporary directory, and builds the same
probe for arm64 and x86_64. The latter uses the Mac's existing Rosetta support.
It never builds or loads the engine codec and never reads an original game
binary. The source input, source hash, library hash, seed and compiler options
are recorded in the output.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tests/voice/regenerate.py \
  --source-archive /absolute/private/path/speex-1.0.4.tar.gz \
  --modern-library /absolute/private/path/libspeex.1.dylib \
  --output /absolute/private/path/regenerated-fixtures.json
```

WS89 regenerated the committed corpus byte for byte, SHA-256
`ff6001e5ee477ce0d3b99f31b17f75394f54aa2e6db7de5dc9a777a1290fed75`.
The tests require neither external library afterward.

## Optional Mac 1.3 observation

`--mac-binary /absolute/private/path/cod2mp_mac_1.3_i386` adds observations from
actual original codec instructions using `mac_reference.py`. This needs an
already available, task-local Unicorn Python package; the runner never installs
it. The oracle accepts only the WS44-pinned binary hash. It executes codec
functions under bounded i386 CPU emulation and does not launch the application.

The loader maps original Mach-O segments in memory, follows original mode data,
and models the needed allocator/imports. Allocations are zeroed, matching the
audited allocator; free is a no-op in the bounded arena. `memmove`/`memcpy`
preserve overlap semantics. Math imports use current host libm through Python's
`math`, with cdecl/x87 return thunks, and random calls use current host libc
`rand`, seeded 17 for each sequence. Diagnostic `fprintf` calls are counted and
suppressed. Unknown imports, nonfinite output, and instruction/time/heap bounds
fail. The helper self-checks cover original RMS, signal multiplication, LSP
interpolation, double-return ABI and overlapping memory movement.

Historical Darwin libm/rand implementations, allocator placement and a full
Mac application environment are not reproduced. The optional report keeps
aggregate PCM errors and model metadata outside Git. Original packet data,
binary bytes, tables, disassembly and decoded original PCM are never embedded
in the fixture. Successful regression tests do not establish original PCM
parity. Two extra bounded original acceptance checks cover mode 5 with
enhancement enabled and mode 6 with enhancement disabled plus two losses.
They require matching status/bit counts, RMSE at most 15 sample units and
maximum error at most 64, for 57 checks when the oracle is enabled. These
limits detect the diagnosed errors while allowing the import-model and scalar
numerical differences already present in the enhancement-off control.
Other enhanced modes retain differences, as recorded in
[the WS91 report](../../docs/macos-port/reports/WS91-voice-original-fidelity.md).
