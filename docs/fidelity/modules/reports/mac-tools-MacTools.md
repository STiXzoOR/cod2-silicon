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
