# Mac/Tools/MacWinSock reconciliation

Updated 2026-11-05.

4/4 reconciled: 3 equal, 1 intentional. The selected Apple provider is src/unix/linux_net.c, present in both native targets; there is no MacWinSock.c source body to audit instead.

Mac closesocket 0x282b0 and ioctlsocket 0x282ba tail-call close and ioctl. Native forwards the same descriptors, pointers and return values. FIONBIO equals the legacy 0x8004667e request on Apple; the conditional request translation changes only non-Apple platforms. WSAGetLastError 0x282f6 adds 10000 for errno values 1 through 88. The four explicit native error cases produce those same values with Darwin errno constants; the remaining range logic matches.

Mac WSAStartup 0x282c4 clears a 400-byte i386 WSADATA then writes its version word. Native clears sizeof(WSADATA), including LP64 vendor-pointer storage/padding, and stores the same low requested version before returning zero. That deliberate typed storage adaptation is not a claim that the native object remains 400 bytes.

No source changed. The arm64 Release dedicated target compiled and linked the native socket provider. No socket operation, new test, full CI or replay was run.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
