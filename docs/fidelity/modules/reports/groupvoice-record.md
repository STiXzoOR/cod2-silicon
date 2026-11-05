# groupvoice/record module reconciliation

Updated 2026-11-05.

**9/9 reconciled: 3 equal at the wrapper boundary, 6 intentional.** Every
exact Mac 1.3 i386 symbol was read by hand. No source correction was needed.

The active native client excludes `src/PC/groupvoice/record.c` and the historical
Mac recorder. `src/platform/macos_voice.c` owns its Record symbols and deliberately
reports microphone capture unavailable. This is the existing WS12/WS110 native
replacement. The dormant engine source was compared separately, not treated as
an implementation of active recording.

`Record_Start` at `0x1ea704` and `Record_Stop` at `0x1ea70e` forward their sample
and return the corresponding DSOUNDRecord result in both wrappers. Native
providers return -1 because capture is unavailable. `Record_DestroySample` at
`0x1ea722` forwards destruction in Mac; native reaches the same native
`DestroyVoice` helper as its DSOUNDRecord destruction provider. This equality
is limited to the wrapper boundary. Native playback samples are owned by the
65-slot voice allocator, so destruction must release their mixer handles even
though capture is disabled; it cannot be replaced with an unconditional no-op.

The other six active boundaries are intentional:

- `Record_NewSample` (`0x1ea718`) returns null because no capture sample can be
  allocated. The historical delegated allocator required initialized recording
  and used a 65-element sample pool with its original 36-byte layout.
- `Record_Init` (`0x1ea9c4`) fails through the disabled native provider, without
  installing a recording callback. Mac installs `Record_AudioCallback` and forces
  initialize one before delegation. Its GOT slot names `current_audioCallback`.
- `Record_Frame` (`0x1ea73e`) does nothing because no recorder exists, rather than
  polling the original recorder and dispatching captured PCM.
- `Record_Shutdown` (`0x1ea72c`) clears native recorder state without destroying
  the encoder. The only production caller, `Voice_Shutdown`, independently calls
  `Encode_Shutdown`; it skips recorder teardown while capture is unavailable.
- `Record_AudioCallback` (`0x1ea9ba`) and
  `Record_QueueAudioDataForEncoding` (`0x1ea748`) return zero in the active
  provider. No callback is installed and `Voice_GetLocalVoiceData` returns before
  recorder polling when capture is unavailable, so these stubs do not discard
  PCM from an active native capture path.

The dormant queue's entire Mac body was compared with the engine source:
gain and average absolute level precede transmission gates; a non-talking call
flushes pending transmission; valid frames reset sample offset and validate
lengths; partial samples are clamped to `frame_size - 4`, copied, then cleared;
source copies use byte offsets; each encoded frame is sent immediately; encoded
byte totals accumulate without advancing `enc_buffer_pos`. The actual
`Client_SendVoiceData` copies each positive packet into client-owned packet
storage and does not change that global. Native typed data supplies 640 shorts
of partial storage and 4096 encoded bytes. The sample descriptor expands from
32 to 40 bytes on LP64 while retaining its named fields. The Mac scaler and
level GOT targets were resolved to the corresponding native voice globals.

The existing exact-size recording queue fixture passed 90 synthetic mono
16-bit cases with ASan/UBSan, covering frame/partial boundaries, gain, gates and
modeled codec/network results. The actual dormant source compiled with client
production flags, as did the active native voice provider in the existing
playback fixture. Playback initialization passed 64 queues, two lifecycles,
unavailable capture, output failure, late packets and the successful-capture
model. The arm64 Release dedicated target also built with two jobs at low
priority. Record triage required a private compile entry copied from the client
encoder flags because CMake excludes this source; its memcpy call-only flag was
checked against all three original copies by hand.

No tests or source were added. The queue fixture did not execute original Mac
instructions: the existing Python environment lacks Unicorn, and no dependency
was installed. Mac comparison here is disassembly and contract review. Invalid
length/frame diagnostic branches were reviewed but are outside the fixture's
finite inputs. Real capture, arbitrary descriptor values, overflowing gain
conversion, actual outgoing codec/network behavior and physical audio remain
unexercised. No recording feature was enabled. Private evidence is in
`~/Library/Application Support/CoD2-native-decomp/recon-groupvoice.noindex/`.

Independent review confirmed the active native boundary and identified a retained
legacy-only mismatch: the two non-COD2_X64 source copies multiply a short-pointer
offset by bytes per sample, whereas Mac 0x1ea863/0x1ea903 uses byte offsets.
The dormant native branches already use byte pointers. The inactive legacy
samplerate/quality import alias also lacks a resolved executable contract.
These pre-existing OFF paths are preserved as required; no equality claim here
certifies the legacy executable or its recording storage.
