# cgame/cg_servercmds_mp module reconciliation

Updated 2026-11-04.

**16/16 reconciled: 9 equal, 4 fixed, 3 intentional.** Read every complete Mac 1.3 body, including the three sound-command callers routed from `com_math`. Decoded the command table at 0x33c6f8 and the six menu cases at 0x33c6e0. The command letters, configstring ranges, menu names, fog constants and registration flags agree.

## Corrections

**Scores changed values that Mac preserves.** Mac 0x1e3218 retains a negative score count; only counts above 64 are capped. Mac 0x1e37e3 updates the client's score even before its client info is valid. Native clamped negative counts and skipped that score update.

**Vote substitution skipped an empty display name incorrectly.** Mac 0x1e3a5f consumes a matched map name even when its display name is empty. It then copies the next input character and checks the consumed-input limit at 0x1e3ab5. Native required a nonempty display name and revisited the next character as another match. The native 256-byte output bound remains: Mac can overrun its local buffer when display names expand the input. No indeterminate stack storage was introduced.

**Invalid sound fades used undefined C conversions.** Mac converts the rounded float to an integer, back to float, and to an integer again. An invalid `cvttss2si` produces INT_MIN and the positive clamp returns zero. Native now checks positivity and the exclusive 2^31 limit before casting. This preserves finite values, NaNs, infinities and overflow without an undefined cast. The three routed callers use the same converter as reverb: Mac 0x1e2976, 0x1e2a38, 0x1e2afe and 0x1e38f5. Music fade arguments are also read in Mac order at 0x1e3524.

## Native boundaries

The established CoD2x client-dvar hook remains before the ordinary three special dvars and dvar fallback in `CG_SetClientDvarFromServer`. Script-menu opening and deferred opening retain their gated native diagnostics; their popup choices, response commands and state transitions match Mac. Existing null and array-bound defenses remain, including rejecting an absent offhand definition and keeping score teams inside the four native team arrays. No platform-specific player-visible distinction was identified that required a Windows override.

## Verification and limits

The arm64 Release dedicated target built at low priority with two jobs. The actual client translation unit compiled separately with its generated production flags. All seven existing server-command sanitizer cases passed, including reverb rounding, room selection, offhand validation and command sequencing. No fixture changes or new tests were needed. Score parsing, vote substitution and invalid fade endpoints were compiled and reviewed but were not exercised by those cases. No full CI or replay ran.

The five legacy preprocessing configurations are unchanged. Neither changed source uses `__LINE__`. Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-protocol.noindex/`.
