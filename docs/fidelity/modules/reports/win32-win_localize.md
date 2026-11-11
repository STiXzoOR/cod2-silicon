# win32/win_localize module reconciliation

Updated 2026-11-05.

**3/3 equal.** All three exact-symbol Mac 1.3 bodies were compared by hand; no source change was needed.

`Win_InitLocalization` (0x774d0) clears both pointers, opens `localization.txt` in `r` mode, obtains size through seek/ftell/rewind, reads into the 4096-byte language buffer, closes the stream, handles a zero read, appends a terminator and replaces the first LF before resolving the language name. The default language remains zero. The original unbounded size behavior is retained; this is not a buffer-hardening claim.

`Win_LocalizeRef` (0x775fe) begins the `localization` parse session, reads alternating key/value tokens, compares the key and reference in that order, ends the session on both exits, and copies the matched value or original reference through `va("%s", ...)`. `Win_ShutdownLocalization` (0x775e4) only clears the two pointers. The file wrappers remain libc delegates in `qcommon/com_fileaccess.c`; no replacement localization backend hides these callers. Relevant cstrings were also checked directly in the binary.

The arm64 Release dedicated target and the actual client compile command for this source file succeeded. No tests were added or changed, and no CI matrix, replay or runtime localization exercise was run. Source is unchanged. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-platform-media.noindex/`.

## Integrated batch verification

Integrated source a3c4c7c passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.
