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

The 48 checks require exact encoded packet bytes and bit counts, matching
decode return values and remaining-bit counts, and a maximum absolute PCM
error of 0.25 relative to independent 1.0.4. This PCM tolerance permits scalar
floating-point differences, including the verified Mac comfort-noise floor;
it is not a claim of bit-exact PCM. The initial-loss noise check and header/
saturation contracts were separately checked against the actual Mac 1.3 decoder.
The one-byte truncated mode-5 payload deliberately retains the original return
value 0 with `speex_bits_remaining == -1`; exhaustion before a complete frame
header returns -1. This is a compatibility fixture, not a general packet-fuzzing
or malformed-payload hardening change.

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
parity: enhancement and some loss/recovery sequences still differ, as recorded
in [the WS89 report](../../docs/macos-port/reports/WS89-voice-compatibility.md).
