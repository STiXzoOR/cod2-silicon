# win32/win_localize module reconciliation

Updated 2026-11-05.

**3/3 equal.** All three exact-symbol Mac 1.3 bodies were compared by hand; no source change was needed.

`Win_InitLocalization` (0x774d0) clears both pointers, opens `localization.txt` in `r` mode, obtains size through seek/ftell/rewind, reads into the 4096-byte language buffer, closes the stream, handles a zero read, appends a terminator and replaces the first LF before resolving the language name. The default language remains zero. The original unbounded size behavior is retained; this is not a buffer-hardening claim.

`Win_LocalizeRef` (0x775fe) begins the `localization` parse session, reads alternating key/value tokens, compares the key and reference in that order, ends the session on both exits, and copies the matched value or original reference through `va("%s", ...)`. `Win_ShutdownLocalization` (0x775e4) only clears the two pointers. The file wrappers remain libc delegates in `qcommon/com_fileaccess.c`; no replacement localization backend hides these callers. Relevant cstrings were also checked directly in the binary.

The arm64 Release dedicated target and the actual client compile command for this source file succeeded. No tests were added or changed, and no CI matrix, replay or runtime localization exercise was run. Source is unchanged. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-platform-media.noindex/`.
