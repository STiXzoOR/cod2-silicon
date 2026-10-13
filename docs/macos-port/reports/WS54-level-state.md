# WS54 — Initial fog and objective state

2026-10-07; base `96f1923`.

FA04: native `G_InitGame` now calls the existing `G_setfog("0")` and
`G_InitObjectives()` immediately after spawning entities and before starting
level scripts. Mac 1.3 calls them in that order at `0x1ae0b8`/`0x1ae0bd`.
The actual referenced string at `0x244a00` is `"0"`, not an empty string.
The initial literal assumption was checked against the binary and corrected
before claiming a reference match.

The real fog function writes configstring 12, sets opaque distance and its
square to `FLT_MAX`, and leaves those defaults when fewer than seven fog
parameters parse. Objective initialization resets all 16 entries, including
entity sentinel 1023. Zeroing the level structure alone supplies neither
sentinel. Existing server mantle animation initialization remains in place.

The `level_state` case in the existing fixes13 runner extracts the actual
post-spawn initialization block and compiles the real fog/objective functions.
It checks fresh and reused state, all objective fields, the configstring and
both fog sentinels, then verifies later script-style fog changes still work.
The original initialization block fails the expected fog write; both corrected
stock and CoD2x pass with AddressSanitizer. This is a production-fragment test,
not execution of the entire level loader or a live dedicated/listen server.

Actual Release `g_main_mp.c` compiles in both modes. Focused cross-TU ABI
checks cover three translation units with zero errors/mismatches in each mode.
Ten legacy preprocessing
configurations have zero differences, and source with native branches removed
is byte-identical to the baseline. Full composed runtime acceptance and the
Linux i386 artifact comparison remain separate checks. Evidence is private in
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/` and
`/tmp/cod2-level-*.log`.

This closes the fog/objective omissions only. Whole `G_InitGame` order is not
certified: the reference loads game-type script before level script
(`0x1ae0f5`/`0x1ae0fa`), whereas the current native function calls them in reverse.
The existing registered-item/weapon save timing also differs. Those order
differences require their own behavior evidence before a separate correction.
