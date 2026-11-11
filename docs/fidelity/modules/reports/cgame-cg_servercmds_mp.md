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

## Integrated batch verification

Integrated source 6b334b3 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
