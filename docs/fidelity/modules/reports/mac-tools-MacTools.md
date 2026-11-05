# Mac/Tools/MacTools reconciliation

Updated 2026-11-05. **16/17 reconciled: 2 equal, 1 fixed, 13 intentional; 1 unchecked.**

`CenterRect` was empty. Mac 0xa409-0xa47d wraps all four extents to signed 16 bits before subtracting and dividing the width/height delta by two toward zero. The Apple native implementation now preserves this arithmetic and wrapped coordinates, including aliased rectangle inputs. Its only source caller is the excluded legacy MacDisplay provider. Compiled and reviewed, not exercised.

The visibility-byte read at 0xa3ee and saved-visibility destructor branch/call at 0xa944 match. The live system cursor is owned separately by SDL input in `unix/linux_input.c`. The old GetCursor/SetCursor, mouse warp/association, PSN search, private Carbon window events, dictionary refresh lookup, IORegistry overloads and cross-process quit event have no external native source or generated-data caller. Native SDL display/input, typed MacSystem hardware queries, engine command/event queues and SDK URL launching replace their respective boundaries. Mac's numeric IORegistry overload has no separate native reconstruction; it is not silently treated as the string overload.

The four alert overloads at 0xa8be/0xa9f0/0xaa14/0xaa9e were read for string conversion, forwarding and release. They are dormant; the CF alert bodies at 0xa7f4/0xa95e were already audited by mac-lifecycle/MacGlobals. Native errors/sys-info use MessageBoxA/stderr and launcher UI. No alert body was edited.

## Unresolved retained sleep ABI

Mac 0xa48c calls UpTime, AddDurationToAbsolute and MPDelayUntil in order. The wrapper source matches that sequence, but project `AbsoluteTime` uses two `UInt32` fields (LP64 unsigned long, 16 bytes) while `macos_compat.c` separately defines two unsigned-int fields (8 bytes). No source/generated-data caller reaches `MacTools_Sleep`, and live scheduling uses MacSystem clocks, but its exported ABI cannot be called equal. Clang record-layout dumps with the actual native client compilation flags independently confirm the mismatch: imported `AbsoluteTime` has size 16/alignment 8 and `hi` at offset 8; the provider type has size 8/alignment 4 and `hi` at offset 4. Matching field names/order alone does not resolve this ABI. Shared type/provider ownership was reported to the parent; no speculative shared-header change was made.

Only Apple `COD2_X64` code changed. Six inactive preprocessor configurations compare identically with base 3d9df23; neither changed file contains `__LINE__`.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
