# WS112 — Restore the native voice playback rate

2026-10-09. Branch `codex/voice-sample-rate`, base `26cf9bb` (WS110 on
release head `7dac4d2`). Code and fixture commit: `a144b89`. The starting
worktree was clean. Scope: incoming voice rate and the recording-rate global;
microphone capture and Config Studio are outside this unit.

Native voice now uses **8192 Hz**, including both invalid-rate fallbacks.
The recording-rate global agrees with the unchanged encoder rate. The native
audio backend and its **48 kHz** output format are unchanged. Both stock and
CoD2x client/dedicated Release builds succeed. The synthetic production-path
test changes from 0.9288125 seconds / approximately 689 Hz to 1.250020833
seconds / 512 Hz for an independently encoded 1.25-second, 512 Hz tone.

## Original behavior and the repair

I read `CONTRIBUTING.md`, `docs/macos-port/PLAN.md`, WS89/WS90/WS91/WS110 and
the voice/audio/platform fixtures before changing engine source. Evidence
comes from the owned full-STABS Mac 1.3 i386 binary, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
`nm -a`, `otool -arch i386 -tV -p <symbol>` and direct Mach-O scalar reads
establish these facts; binary bytes, original PCM and disassembly are absent
from this commit.

| Reference | Address | Rate behavior |
| --- | --- | --- |
| `g_encoder_samplerate`, `g_sound_recordFrequency` | `0x34d7cc`, `0x34d7ec` | Both initial values are 8192. `g_sound_channels` at `0x34d7e4` is 1. |
| `Sound_Init`, `DSound_Init` | `0x1eafb0`, `0x1f893e` | The wrapper delegates; original DSound initialization sets its initialized flag rather than choosing another playback rate. |
| `DSOUNDRecord_NewSample` | `0x1f858e` | Copies the recording-rate global into sample `frequency` at `+0x8`, channels into `+0x14`, and sets two bytes per sample. |
| `DSound_NewSample` | `0x1f888e` | Obtains that sample, initializes its Miles handle and passes `frequency` to `AIL_set_sample_playback_rate` at `0x1f88fe`. Mono PCM16 uses Miles format 1. |
| `Voice_IncomingVoiceData`, `Voice_Playback`, `DSound_SampleFrame` | `0x1ea43c`, `0x1ea3cc`, `0x1f8976` | Decode and queue PCM, then service samples. There is no intervening voice-rate conversion. |
| `CSoundObject::set_sample_playback_rate`, `FormatToDescription` | `0x13e3bc`, `0x13e7a8` | Retain the supplied rate and convert it to the source ASBD sample rate. |
| `CSampleSound::UpdateStreamFormat`, `DoRender` | `0x1142c8`, `0x1146dc` | Read the mixer format and create/use an AudioConverter when conversion is needed. This converts from the configured 8192 Hz source to the mixer format. |

Thus the original **does resample at the output layer**. It does not relabel
8192 Hz decoded frames as 11025 Hz. A 160-sample frame represents
19.53125 ms; rendering it at 11025 Hz accelerates it by 11025/8192, approximately
1.3458. This repair changes only the native global's initializer and the two
fallback values in `src/platform/macos_voice.c`, with a reference comment.
Existing positive-rate overrides remain supported.

CoD2x 1.4.6.8 `src/mss32/hook.cpp:327–335` deliberately disables Windows
microphone registration and the call to `CL_VoicePacket` to avoid its mixer
hang. Its Miles playback-rate exports forward to the original library; no
voice-rate or codec conversion patch was found in the client/shared hooks.
The existing WS110 native incoming-voice adaptation remains in both modes,
as required by this brief. No CoD2x policy or `COD2X_HOST_REVISION=0` change
was made.

## Synthetic red/green test

`tests/voice/tone_packets.json` contains 64 packets produced by unmodified
[Xiph Speex 1.0.4](https://downloads.xiph.org/releases/speex/speex-1.0.4.tar.gz),
using the same archive SHA-256 pinned by WS89:
`d0708ab9d1a6ae6aa8f59cf7b8c37c1cf03fc2c2fe20401a03b08c57c9898630`.
The input is an independently specified integer period-16 sine at 8192 Hz,
amplitude 8000, narrowband mode 5, seed 17. Neither original PCM nor native
decoder output is used as an expectation. `tone_packets.py` reproduces the
packets byte-for-byte; fixture SHA-256 is
`582563a2cfbfb5a35c857a496d67843281f52df2f22403f9393aa35f6135e57f`.
The existing WS89/WS91 corpus remains byte-identical, SHA-256
`ff6001e5ee477ce0d3b99f31b17f75394f54aa2e6db7de5dc9a777a1290fed75`.

The new `--rate-contract` mode of `tests/voice/playback_init.py` builds
production `Voice_Init`, `CL_VoicePacket`, message readers, Speex, decode and
playback wrappers, the native voice adapter and audio backend with actual
Release flags, assertions, ASan and UBSan. It delivers the fixed packets to
one talker, invokes `Voice_Playback` between 128-frame render callbacks and
measures actual emitted stereo float PCM. The independent expectation is
64 × 160 / 8192 = 1.25 seconds = 60000 output frames, with one frame allowed
for cursor rounding. A Hann-window spectral peak search over 400–800 Hz,
refined to 0.125 Hz, requires 512 Hz within 0.25 Hz.

The initial test was red in **both modes before the production edit**.
After strengthening the backend boundary, `--baseline 26cf9bb` reproduced
the same failure through the real native digital-driver function in both
modes; final current-source runs pass:

| Source, both modes | Default global | Zero global | Negative global |
| --- | --- | --- | --- |
| Baseline 26cf9bb | 44583 frames; 0.9288125 s; 689 Hz; duration/pitch and rate consistency fail | Same duration/pitch failure | Same duration/pitch failure |
| Repaired | 60001 frames; 1.250020833 s; 512 Hz; pass | Same rendered result; pass | Same rendered result; pass |

The driver bridge supplies a test-owned existing-output-unit sentinel, then
calls the real driver function. Format validation and its shared default-rate
update run; hardware creation is skipped. Real sample allocation, PCM type,
playback-rate setting, buffering, resampling and `Render` remain active.
The unavailable recorder runs unchanged. Dvar registration, mute lookup and
device existence are controlled boundaries; no game or UDP socket runs.

An existing non-voice 48 kHz sample retains its rate and exactly 480 frames
of constant stereo PCM before voice startup, after startup and after shutdown.
A newly allocated game sample after startup passes the same check. Existing
audio positioning, timelines and all 138 footstep cases also pass in both
modes. These are bounded output checks, not full original Miles acoustics,
resampler, physical-device latency or PCM equivalence. WS91's remaining
enhanced-mode differences remain open. Microphone capture remains unavailable.

## Verification and gate status

Private evidence:
`~/Library/Application Support/CoD2-native-current-readiness/ws112.noindex/`.
Exact commands/statuses are retained in `verification-results.json`,
`rate-backend-results.json`, `gate-results.json` and `build-results.json`, with
named logs and fixture receipts. All compilation/heavy checks use
`taskpolicy -b nice -n 19`; builds use one job and no more than two compiler
processes run concurrently within this unit. Focused ABI databases contain
the changed production TU, so the checkers' larger executor defaults do not
increase compiler concurrency.

| Check | Stock | CoD2x |
| --- | --- | --- |
| Release client and dedicated build; typed snapshot verification | Pass | Pass |
| Final rate test, real backend, three rate cases, ASan/UBSan | 3/3 | 3/3 |
| WS110 initialization/receive/render fixture | Pass | Pass |
| Voice codec and compatibility contracts, ASan/UBSan | 55/55 | 55/55 |
| Environment command/reverb | Pass; 10 reference cases, zero differences | Same |
| Positional/timeline/138-footstep output fixture | 4/4 cases | 4/4 cases |
| Platform hardware voice/audio API fixtures, ASan/UBSan | 2/2 | 2/2 |
| Host lifecycle, listen and dedicated | Pass | Pass |
| Host protocol and challenge-random controls, both targets | Zero failures | Zero failures |
| Production UDP operations including voice dispatch, both targets | Zero failures | Zero failures |
| Focused function ABI, changed TU | Zero errors/mismatches/new findings | Same |
| Focused raw-offset/import ABI | Zero sites/unreviewed offsets; zero active import sites/errors | Same |
| Focused stub gate | 10 existing candidates, zero new | Same |
| Actual SDK preprocessing with native definition removed | Zero mismatches/errors | Same |

Both linked clients contain `g_sound_recordFrequency=8192`,
`g_encoder_samplerate=8192` and `g_sound_channels=1`; binary hashes and scalar
reads are in `final-binaries.json`. `tests/fixes13/legacy.py --base 26cf9bb`
reports zero differences across ten configurations for the engine file.
`tools/abi/legacy.py --base 26cf9bb` reports zero across five native-off
configurations, including the changed test expectation. Explicit
`unifdef -UCOD2_X64` plus `cmp` is byte-identical for the **only changed engine
file**. `git diff --check` passes. These do not prove Linux i386 object/binary
identity; x86 Linux multilib is unavailable on this Mac.

The four ABI unit suites pass 17/17 tests; fidelity tests pass 35/35 and
datagen tests 12/12. Trace guards find zero unguarded diagnostics in 726 files.
The first tools suite attempt was **31/32**, with
`test_benchmark_completion_and_shutdown` returning `Operation not permitted`.
An isolated synthetic diagnostic and a full unchanged rerun pass; the final
tools receipt is **32/32**. The first failure remains recorded and its cause
is unlocalized; no harness behavior or expectation was changed to pass it.

Known red gates remain red, without baseline or policy changes:

- Release-baseline `tools/abi/legacy.py --base 664f1a1`: **10 differences**, the
  five generated-data configurations excluded by CMake OFF and five test-only
  server-control configurations.
- Release-baseline inactive CoD2x check: **463 before**, **465 after**, across
  17 configurations (95 then 96 modified files). The added two are this
  engine file under native stock and synthetic Apple i386 with `COD2_X64=0`
  defined. Native stock intentionally receives the repair; real CMake OFF
  omits the definition. The historical brief's 436 was already 27 below the
  observed pre-unit count; that inherited difference is not attributed here.
- Unit-only `tests/cod2x/check_inactive_gates.py --base 26cf9bb`: **4
  differences**: the two above plus the updated initialization fixture's rate
  expectation under the same two configurations. That fixture was added by
  WS110, so the release-baseline checker excludes it as an added file.
- Linux multilib object/binary comparison: unavailable, not a passing result.

Reproduce the central checks from this tree with fresh output paths:

```sh
E="$HOME/Library/Application Support/CoD2-native-current-readiness/ws112.noindex"
for mode in stock codx; do
  taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py --rate-contract \
    --compile-db "$E/$mode-compile-commands.engine.json" \
    --output "$E/reproduce-rate-$mode"
  taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py --rate-contract \
    --baseline 26cf9bb --compile-db "$E/$mode-compile-commands.engine.json" \
    --output "$E/reproduce-red-$mode" # expected exit 1
  taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py \
    --compile-db "$E/$mode-compile-commands.engine.json" --output "$E/reproduce-init-$mode"
  taskpolicy -b nice -n 19 python3 tests/voice/run.py --sanitizers \
    --compile-db "$E/$mode-compile-commands.engine.json" --output "$E/reproduce-codec-$mode.json"
done
```

Fresh full builds use `taskpolicy -b nice -n 19 cmake -S . -B build-ws112-<mode>.noindex
-DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, adding `-DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1`
for CoD2x, then `taskpolicy -b nice -n 19 cmake --build <build> --target
cod2_macos cod2_macos_ded --parallel 1`. Save `compile_commands.engine.json`
before build-dependent fixture commands. The remaining exact commands are
in the receipts and private `verify.py` / `verify_gates.py` scripts. Recreate
those build directories before repeating checks that use their working
directory. Full integration merge gates remain the orchestrator's task.

## Handoff

Compose `a144b89` and the following report commit **on top of WS110
`26cf9bb`**. This unit changes one engine file and the voice fixtures/docs.
The session was locked (`CGSSessionScreenIsLocked=Yes`), so no optional game
smoke, game window, benchmark lease or QA app was created. Platform hardware
API checks are separate from live gameplay. No capture/TCC, global config,
game data, remote, push, tag, PR or merge operation occurred.

Task-created build directories and fixture scratch binaries/objects/caches
are removed after saving receipts and compile databases. Evidence remains
only under the named private directory. No owned process or app bundle remains.
The final worktree has no uncommitted changes.
