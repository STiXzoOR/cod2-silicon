# Mac/Tools/CCircularBuffer reconciliation

Updated 2026-11-05.

8/8 reconciled: 2 equal, 3 fixed, 3 intentional. Read all eight exact Mac entries. Native Apple selects src/platform/macos_ring.c and typed pointer/UInt32 fields, excluding the old source's pointer-to-int storage and empty Write stub.

Mac ReadPtrSize 0x1f9636 and ReadPtr 0x1f965e add the previous pending-read length and reset to zero when the resulting position reaches capacity. Native used modulo; restored the exact reset rule and request/span bookkeeping. Mac Write 0x1f9784 allows the full contiguous free span, then optionally a second span before readOffset. Native reserved one byte and clamped a modular used count. It now follows the Mac branches and updates, including the original equal-index full/empty ambiguity. Native zero-capacity guards remain defined.

The default constructor 0x1f95ee and Reset 0x1f9618 match initialized field behavior. The destructor and Alloc keep an explicit native allocation adaptation: always reset storage and record zero capacity on malloc failure, rather than retaining a nonzero capacity with a null buffer. This does not introduce indeterminate native storage.

The size-taking constructor at 0x1f9740 zeroes all fields, allocates, and records the requested capacity. Follow-up exact-entry caller analysis identifies DSound_NewSample 0x1f888e: its call at 0x1f88e0 constructs a 51,200-byte playback ring. The selected native DSound_NewSample in macos_voice.c replaces that storage with the owned MacVoiceSample queue and complete native playback lifecycle. This overload is intentionally retired with that mapped owner; it is not equated to the native default constructor or Alloc. The alias/reference audit, queue-capacity adaptation, and capture distinction are documented in [the focused report](mac-ring-sized-constructor.md).

The arm64 Release dedicated target compiled and linked the changed provider. The corrections were reviewed against the disassembly but not exercised; no new tests, full CI or replay ran. OFF preprocessing matches the base, and the file contains no __LINE__ expansions.

Parent ran the unchanged existing Debug ASan/UBSan threads_ring fixture after correcting its SDL target dependency: recursive mutexes, native IDs, virtual destructors,20 dual-stop restarts and10000 wrapped bytes passed. Full/empty ambiguity and allocation failures were not directly exercised. No new tests were added.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.

The size-taking-constructor follow-up above postdates that integrated batch and closes its ring boundary. This report-only follow-up does not rerun the batch's validation.
