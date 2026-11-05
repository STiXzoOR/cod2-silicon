# Mac/Tools/i5stdlib reconciliation

Updated 2026-11-05.

4/4 assigned rows equal. Mac stricmp 0x258f8 and strcmpi 0x2590c tail-call strcasecmp; strnicmp 0x25902 tail-calls strncasecmp. Native uses the same libc calls and returns their results. The native length type widens with size_t.

Mac strupr 0x25954 walks through NUL, sign-extends each char into __toupper, stores the result byte and returns the original pointer. Native has that same signed-char loop on Apple. No claim is made that Windows locale conversion has identical non-ASCII behavior; these are the retained Mac libc helpers.

The engine itoa at 0x25994 belongs to this source but is routed to punkbuster/pbsv in the ledger; its correction is documented there. The source compiled in the arm64 Release dedicated build. No new test or runtime exercise ran.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
