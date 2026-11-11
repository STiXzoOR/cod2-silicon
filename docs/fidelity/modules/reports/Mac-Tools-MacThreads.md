# Mac/Tools/MacThreads reconciliation

Updated 2026-11-05.

13/13 reconciled: 7 equal, 6 intentional. Restarted exact-symbol disassembly for all entries because the full-binary listing misaligned the CMutex destructor. Native Apple selects src/platform/macos_threads.c and excludes the legacy raw-offset source.

The CMutex destructor at 0x11734c, virtual ExecuteProc at 0x1173d8, running predicate at 0x1173cc and four lock-guard constructor/destructor entries match their lifetime operations through typed native pointers and SDK mutex fields. ExecuteProc calls slot 2 with the saved argument and clears running afterward; native uses release/acquire accesses. Guard destructor flag clearing has no observer after the valid guard lifetime.

Mac Run 0x117360 stores its argument and running byte before pthread_create and clears running on failure. Native retains those outcomes but serializes lifecycle ownership and joins a completed previous worker. Mac Stop 0x1173b0 and destructors 0x1174ee/0x117536/0x11757e clear running and wait a fixed 100 ms; native joins its owned thread and destroys a separate lifecycle mutex. The constructor 0x11747a uses a native vtable and typed fields instead of the i386 offsets. These are explicit SDK lifetime adaptations. Native audio and voice own their own state; the old Miles/Carbon consumers are excluded. No callsite was found that combines typed native CThread storage with an old offset-based consumer.

The mutex constructor is attributed to MacExceptions, outside these 13 rows. Its exact Mac body at 0x1173fe throws a signed 32-bit status when pthread attribute initialization, recursive-type selection or mutex initialization fails. The current native provider ignores those errors. This was reported to the MacExceptions owner and parent; this report does not claim its failure contract is reconciled or invent a cross-language throw ABI.

No source changed. The typed provider compiled and linked in the arm64 Release dedicated target, but no thread or mutex failure path was exercised. No new tests, full CI or replay ran.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
