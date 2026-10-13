# WS110 — Initialize native voice playback without microphone capture

2026-10-09. Branch `codex/voice-playback-init`, base
`7dac4d2b054240bd487e9aa3e5a4d6390b010c3d`. The starting tree was clean.
Scope: incoming voice initialization, capture exclusion, and the wrapper safety
issues exposed by exercising that previously unreachable native path.

## Result and reference basis

Native `Voice_Init` now initializes playback, the narrowband encoder/decoder,
and 64 playback samples when the native recorder is unavailable. Capture
polling, local voice-level sampling, and microphone mixer setup/update/restore
remain disabled in that case. The existing one-time stderr notice is unchanged.
The historical zero return value is retained: the original caller does not use
it as an initialization-success flag. `g_voice_initialized` remains the playback
capability flag. A failed output-device initialization leaves voice inactive;
incoming data is ignored while inactive, including after shutdown.

I read WS89, WS90, WS91 and the existing voice/audio/platform fixtures before
changing production code. The owned Mac 1.3 i386 reference was inspected with
`nm -a` and `otool -arch i386 -tV -p <symbol>`. Original instructions and data
remain in the private evidence directory, not this repository.

| Original function | Address | Finding |
| --- | --- | --- |
| `Voice_Init` | `0x1ea1b0` | Calls `Record_Init` at `0x1ea273`; zero returns immediately at `0x1ea27c`, before `Sound_Init`, either codec, initialization flag, or playback samples. |
| `Record_Init` | `0x1ea9c4` | Installs the audio callback, then calls `DSOUNDRecord_Init`. |
| `DSOUNDRecord_Init` | `0x1f85f6` | Constructs a 16-bit recorder with the global rate/channels. Failed initialization destroys and clears it and returns zero; success sets the recording flag. |
| `Sound_Init` | `0x1eafb0` | Independently delegates to `DSound_Init(1, handle)`. |
| `Voice_Playback` | `0x1ea3cc` | Requires `g_voice_initialized`, updates microphone level, then services all playback samples and sound frame. |
| `Voice_IncomingVoiceData` | `0x1ea43c` | Updates the remote talk timestamp, decodes into PCM and updates that talker's playback sample. No recording-object dependency and no original initialization guard. |
| `CL_VoicePacket` | `0x1478f2` | Parses packet count, talker and byte length; filters muted players and `cl_voice`; calls incoming-data processing. No capture or initialization gate. |
| `Voice_GetLocalVoiceData` | `0x1ea65c` | Requires initialization, lazily starts recording, applies the scaler, and polls recording each client frame. |
| `Voice_IsClientTalking` | `0x1ea584` | The remote indicator lasts through elapsed 299 ms. |

Thus original initialization couples playback to capture success even though
the playback operations themselves are independent. This fallback is an
explicit adaptation for the native unavailable-recorder stub, authorized by
this brief; it is not asserted to be the original failure policy. The original
capture-success statement order is retained. All production changes use
`__APPLE__ && defined(COD2_X64)` gates.

CoD2x 1.4.6.8 `src/mss32/hook.cpp:327–335` deliberately returns early from
microphone-function registration and removes the Windows call to
`CL_VoicePacket`, documenting a Windows mixer enumeration hang. This is a
Windows workaround, not a new wire format or playback algorithm. In accordance
with the brief's requirement to restore native incoming voice in both modes,
that Windows failure workaround is not newly applied to macOS. No CoD2x hook,
host revision, protocol or server voice policy is changed.

## Red tests and the necessary wrapper repairs

The new `tests/voice/playback_init.py` compiles production `win_voice.c`, the
native voice adapter, playback/encode/decode wrappers, message readers, all 15
Speex units, and the native audio implementation with actual engine flags,
assertions, ASan and UBSan. It extracts the unchanged production
`CL_VoicePacket`, `IN_KeyDown`/`IN_KeyUp`, and talk-command bodies. Its packet is
the existing independently encoded synthetic mode-5 packet in `fixtures.json`;
that corpus is unchanged.

No explicit `DSound_Init` is called by the test. The native `Record_Init`
implementation actually fails and prints its notice. Only output-device
creation is replaced for deterministic in-memory rendering. Dvar registration,
command arguments, mute lookup, and the separate successful-capture control
are fixture boundaries. Decode, queue ownership, sample buffering, resampling
and the actual `Render` callback remain production code.

- Against `--baseline 7dac4d2`, after passing data through `CL_VoicePacket`, the
  initialization assertion fails: voice flag/decoder/frame size remain unset.
  Receipt: `red-stock-4/result.json`, expected assertion abort, one notice.
- Allowing initialization first exposes an ASan four-byte read of the one-byte
  `yes` variable in `Encode_SetOptions`: `green-stock-1/run.log`. The native
  argument is now an integer. Original `Encode_SetOptions` at `0x1eab70` also
  stores a byte, while original narrowband control requests 30/34 jump to
  `0x1f6c5e`/`0x1f6c8c` and read four bytes. This is an explicit removal of an
  undefined overread while retaining enabled VAD/DTX intent, not a claim that
  the original caller had a different declaration.
- Next, UBSan detects conversion of an uninitialized unused float in
  `Decode_Sample`: `green-stock-2/run.log`. Original `Decode_Sample` at
  `0x1eaaec` likewise loops over `frame_size * 2` shorts while returning that
  number as PCM **bytes**. Speex produced only 160 floats; playback consumes
  320 bytes, i.e. the first 160 shorts. Native conversion now stops at
  `frame_size`, retaining exactly the consumed output and byte count. A sentinel
  check independently requires the unused following 160 shorts to stay intact.
- A new late-packet case then exposes freed Speex bits after `Voice_Shutdown`:
  `red-late-packet/run.log`. The native incoming-data initialization guard
  prevents this use-after-free and also covers failed output startup.

Final fixtures require finite, nonzero render output from all 64 talkers,
mute/`cl_voice` filtering, the 299/300 ms talk-indicator boundary, 100 frames
while `+talk` is held, unchanged local packet storage, zero local voice level,
zero capture/microphone mixer calls while unavailable, two full init/shutdown
cycles, output-startup failure, late packets, and exactly one stderr notice.
The synthetic successful recorder checks aggregate capture/mixer call counts;
statement-order preservation is established by source inspection, not those
counts alone.

These are bounded receive-to-mixer checks. They do not exercise connectionless
command dispatch, a UDP socket, full game startup, physical sound, real capture,
or original PCM parity. WS91's remaining codec differences remain open.

## Verification

Validation receipts and exact command arguments are in `verification-results.json`
and the named logs under the private evidence directory. All compilation and
heavy checks run under `taskpolicy -b nice -n 19`, with no more than two compiler
jobs. The independent read-only review found no critical or important issues;
its two minor harness/documentation points were addressed.

| Check | Stock | CoD2x |
| --- | --- | --- |
| Release `cod2_macos` and `cod2_macos_ded`, typed snapshot verified | Both build | Both build |
| Reviewed receive-to-render fixture, ASan/UBSan | Pass, including arguments-only compile database | Pass |
| Existing `tests/voice/run.py`, contracts and compatibility probe, ASan/UBSan | 55/55 | 55/55 |
| Environment/reverb audio contracts | Pass; 10 reference cases, zero differences | Same |
| Positional/output/timeline/138-footstep audio fixture | 4/4 cases | 4/4 cases |
| Platform voice/audio API fixtures, ASan/UBSan | 2/2 | 2/2 |
| Host lifecycle, listen and dedicated | Pass | Pass |
| Host protocol, listen/dedicated and challenge-random controls | Pass | Pass |
| Production UDP operations, including voice dispatch | Zero failures, both targets | Same |
| Focused function ABI, four voice TUs | Zero errors/mismatches/new findings | Same |
| Focused raw offsets, four voice TUs | Zero sites/unreviewed offsets | Same |
| Focused import ABI, four voice TUs | Seven active sites, zero errors | Same |
| Focused stub gate, four voice TUs | 10 existing candidates, zero new | Same |

The four ABI fixture unit suites pass 17/17 tests; `tools/tests` passes 32/32,
`tools/fidelity` passes 35/35, and trace guards report zero unguarded diagnostics
in 726 files. `tests/fixes13/legacy.py --base 7dac4d2` reports zero differences
across ten configurations for the three changed engine files.
`tools/abi/legacy.py --base 7dac4d2` reports zero across five native-off
configurations. `unifdef -UCOD2_X64` followed by `cmp` is byte-identical for
**each** changed engine file. Six sequential actual SDK preprocessing
comparisons, three files in both modes with the native definition removed,
also report zero mismatches/errors. `git diff --check` passes. The synthetic
corpus SHA-256 remains
`ff6001e5ee477ce0d3b99f31b17f75394f54aa2e6db7de5dc9a777a1290fed75`.

The initial import-check invocations omitted the required `--binary` argument
and exited 2 before checking anything. Final corrected import receipts are
recorded separately; those invocation errors are not treated as passes. The
private `imports_serial.py` invokes the unchanged import checker with its
executor limited to one worker, preserving validation policy while respecting
the shared-Mac concurrency limit. Redundant per-source import runs were stopped
and batched to avoid repeatedly parsing the complete STABS reference.

Representative reproduction commands (use fresh output paths):

```sh
WS110_EVIDENCE="$HOME/Library/Application Support/CoD2-native-current-readiness/ws110.noindex"
for mode in stock codx; do
  taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py \
    --compile-db "$WS110_EVIDENCE/$mode-compile-commands.engine.json" \
    --output "$WS110_EVIDENCE/reproduce-init-$mode"
  taskpolicy -b nice -n 19 python3 tests/voice/run.py --sanitizers \
    --compile-db "$WS110_EVIDENCE/$mode-compile-commands.engine.json" \
    --output "$WS110_EVIDENCE/reproduce-codec-$mode.json"
done
taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py \
  --compile-db "$WS110_EVIDENCE/stock-compile-commands.engine.json" \
  --baseline 7dac4d2 --output "$WS110_EVIDENCE/reproduce-red"
```

For fresh full builds, configure `build-ws110-stock.noindex` and
`build-ws110-codx.noindex` using `-DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release
-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, adding
`-DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1` for CoD2x. Build each sequentially with
`taskpolicy -b nice -n 19 cmake --build <build> --target cod2_macos
cod2_macos_ded --parallel 2`. Save the original database as
`compile_commands.engine.json`. The exact remaining audio, platform, host,
UDP, ABI and preservation commands are in `verification-results.json` and
`verify_remaining.py`; final import commands use `imports_serial.py --binary
<reference> --compile-commands <mode>-voice-db.json --json <receipt> --check`.
No test tolerance, expected codec corpus, gate baseline or host revision is
changed; `COD2X_HOST_REVISION=0` stays locked.

The x86 Linux multilib object/binary comparison cannot run on this Mac. The
native-off source/SDK comparisons support guard preservation; they do not
establish byte-identical legacy objects or binaries. The inherited red gates
must remain visible: legacy ABI has 10 known differences (five generated-data
configurations excluded by CMake OFF and five test-only server-control
configurations); the brief recorded 436 inactive CoD2x differences across
intentional stock repairs and synthetic defined-zero native configurations.
Neither policy is weakened here. Broader integration gates remain the
orchestrator's responsibility.

The readiness-wave comparison uses release baseline `664f1a1`.
`tools/abi/legacy.py --base 664f1a1` reproduces the known 10 differences across
94 modified files. A separate historical source-only probe against upstream
`410342a` reports 139 pre-existing differences across 235 files; it is retained
as `legacy-abi-aggregate.log`, not confused with the release-baseline count.
The redundant oldest-upstream inactive-gate run was cancelled when switching
to the readiness baseline; it is not a passing result. This unit's delta
`tests/cod2x/check_inactive_gates.py --base 7dac4d2` is **red: six differences**,
two for each changed engine file: native stock and synthetic Apple i386 with
`COD2_X64=0` defined. The repairs intentionally apply to native stock, and
CMake native OFF omits the definition. That explains the delta without
weakening the checker or representing it as green.

The fresh release-baseline inactive check is **red: 463 differences** across
95 files and 17 configurations (`inactive-gates-release.log`). Applying the
unchanged checker's configurations and preprocessing rules to the three
changed files attributes six to WS110 and infers **457 before this unit**
(`inactive-count-attribution.json`). That pre-unit count is already 21 above
the brief's historical 436; those 21 are not attributed to individual causes
here. The current aggregate is 457 + 6, not a passing gate or the older count.

The optional live listen-server/client check was skipped because
`ioreg -n Root -d1` reported `CGSSessionScreenIsLocked=Yes`. No game process,
window, benchmark lease or QA app bundle was created. Hardware audio fixture
results, when listed, are separate from live game acceptance.

## Microphone capture proposal — no implementation in this unit

The original initial data is **8192 Hz**, mono, signed 16-bit PCM, not an assumed
8000 Hz: `g_encoder_samplerate` at `0x34d7cc` and
`g_sound_recordFrequency` at `0x34d7ec` are both 8192; channels at `0x34d7e4`
are 1; encoder quality starts at 1. The narrowband frame has 160 samples,
therefore 19.53125 ms at that configured rate. `DSOUNDRecord_Frame` at
`0x1f86f2` supplies byte count, sample count, two bytes per sample, rate/channels,
and zero sample offset to the encode callback. The recorder uses an AudioUnit
and an AudioConverter, drains a circular buffer on client-frame polling, and
resets conversion/buffering on overflow. Hardware callback sizes must not
become codec or network frame sizes.

There is an inherited native discrepancy: `macos_voice.c` currently initializes
the playback/record-rate global to **11025 Hz**, while the native encoder's
typed initial data is 8192. This unit deliberately does not change the rate.
Its audible-output fixture uses the current 11025 Hz backend; it proves
initialization and output, not original pitch/duration fidelity. Before adding
capture, independently validate and reconcile recording, encoder and playback
rates against the original, with duration/pitch tests. Do not label incoming
voice acoustics faithful merely because this initialization repair passes.

Proposed follow-up:

1. Use a native **AUHAL input AudioUnit plus AudioConverter**, retaining the
   existing output backend. It fits the C platform layer, macOS 13 deployment
   floor, and original recorder's separation of callback buffering from
   conversion/polling. AUHAL must run at the device's rate; convert buffered
   input to the reference mono PCM16 rate outside the realtime callback.
   [Apple's TN2091](https://developer.apple.com/library/archive/technotes/tn2091/_index.html)
   documents that device-rate constraint and the separate converter approach.
   `AVAudioEngine` with an input-node tap and `AVAudioConverter` is a viable
   higher-level alternative, but adds an Objective-C engine graph and more
   scheduling/reconfiguration behavior to validate.
   [Apple input-node documentation](https://developer.apple.com/documentation/avfaudio/avaudioengine/inputnode)
   and [TN3136](https://developer.apple.com/documentation/technotes/tn3136-avaudioconverter-performing-sample-rate-conversions)
   describe those building blocks. This API preference is an engineering
   proposal, not a demonstrated acoustic equivalence claim.
2. Preallocate a bounded ring. The realtime callback only copies device PCM
   and metadata; conversion, level calculation, encoding and network work
   occur outside it. Preserve a partial-frame accumulator across game frames;
   encode exactly 160 samples each time. Reproduce original overflow reset,
   stop/start and teardown ownership with tests. Handle no input device,
   unplug/default-device changes, changed hardware rate, denial and later
   revocation while keeping playback alive. Do not add automatic gain control,
   echo cancellation or voice processing without reference evidence.
3. Restore the real encode callback plumbing. CMake excludes
   `src/PC/groupvoice/record.c`; native `Record_Init`, frame polling,
   `Record_AudioCallback` and queue-for-encoding are currently stub adapters.
   Implementing only `DSOUNDRecord_Init` will not transmit anything. Audit the
   reconstructed byte/sample indexing and partial-frame handling against Mac
   instructions before enabling that path. Compare synthetic PCM-to-packet
   bytes and timing against the independent codec and original bounded oracle.
4. Preserve dvars and push-to-talk semantics. `winvoice_mic_reclevel` has range
   0–65535, default 65535; the Mac setter normalizes by 65535 and scales the
   device's saved input volume. Prefer faithful supported device-volume control
   with save/restore, and explicitly define behavior for devices without it.
   `winvoice_mic_scaler` registers 0.25–2 but the active scaler clamps to
   0.5–1.5. The original `winvoice_mic_mute` adapter is counterintuitive:
   value 1 calls recorder mute with **false**; value 0 zeros the converter's
   input bytes. Preserve that inversion, rather than infer behavior from the
   name. Original local polling starts capture even outside `+talk`; actual
   transmission additionally requires server/client voice enabled, rate at
   least 5000, active connection, and talking/pending voice. The client batches
   at elapsed >199 ms or more than nine queued packets. Maintain those gates
   and test hold/release and partial-frame boundaries.
5. Add an asynchronous permission/capture-capability state machine. Request
   microphone access on explicit user intent to enable recording or use
   push-to-talk, and keep receive playback running during a pending prompt,
   denial or unavailable hardware. The existing capture boolean is initialized
   synchronously; a future grant needs an explicit way to attach capture after
   playback initialization without recreating the decoder or losing queues.
   Explain the macOS microphone indicator and denied-state recovery in product
   UI. Verify permission ownership for the actual engine helper, not merely
   the launcher.

Packaging requirements: include a meaningful localized
`NSMicrophoneUsageDescription`, e.g. “Use your microphone for multiplayer voice
chat when you hold the talk key.” macOS presents its system microphone-access
prompt on first request, using the application identity and purpose string;
denial must leave incoming voice available. The current packager creates a
nested `Contents/Helpers/CoD2 Game.app` with a separate `.game` bundle ID and
ad-hoc signs it; it does not enable hardened runtime. The capture executable's
bundle needs the usage string, including the engine-only packaging variant.
Test whether the supported launcher/helper launch route attributes the prompt
to the intended app, with stable signed identities. Do not rely on command-line
permission behavior as packaged-app evidence.
[Apple's macOS capture authorization guidance](https://developer.apple.com/documentation/bundleresources/requesting-authorization-for-media-capture-on-macos)
describes the usage key and explicit authorization.

For a future hardened-runtime/notarized build, sign the capturing executable
with `com.apple.security.device.audio-input`; sandboxed packaging also needs
the appropriate microphone sandbox capability. Audit nested helper signatures
and retain required library loading behavior; do not add broad unrelated
exceptions. No hardened-runtime change is needed merely to restore playback.
[Apple audio-input entitlement documentation](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.device.audio-input)
and [hardened-runtime guidance](https://help.apple.com/xcode/mac/current/en.lproj/devf87a2ac8f.html)
support those packaging requirements. Exact prompt identity, hardware gain
availability, converter delay/rounding and device-change behavior remain
follow-up risks requiring packaged runtime checks.

No Info.plist, entitlement, TCC state, global configuration, dependency install,
or microphone implementation was changed.

## Handoff

Evidence: `~/Library/Application Support/CoD2-native-current-readiness/ws110.noindex/`.
Only this worktree and its private evidence were written. Licensed data was
not modified. No push, tag, PR, remote operation or merge was performed.
Both task-created build directories were removed after saving build logs,
binary hashes and compile databases. Fixture scratch binaries, headers,
objects and audit caches were also removed; logs and JSON receipts remain.
No owned test process or QA app bundle remains. `final-verification.json`
collects the outcomes, including failed/cancelled intermediate invocations.

Compose code commit `a2c3e7b` and the following report-only commit from this
branch onto the release integration lineage. The complete unit changes three
engine files, two new fixture files, the voice README and this report. The
final worktree is clean. Microphone capture, rate reconciliation, outstanding
codec parity and the red aggregate gates remain follow-up work.
