# cgame/cg_hudelem_mp module reconciliation

Updated 2026-11-05.

**8/8 reconciled: 4 equal, 4 fixed.** Every assigned body was compared by hand with Mac 1.3, with call/string triage, literal checks and decoded timer jump tables as supporting evidence. The full-binary disassembly misaligned `CG_AreHudElemsHidden`; disassembly restarted at its exact symbol confirms its gates. Native changes are guarded; the five inactive preprocessing configurations retain the original source bodies. Neither source file uses `__LINE__`.

## Corrections

**Both draw passes read the wrong snapshot and collected equal-sort elements in the wrong order.** Mac 0x1e11c4–0x1e122f and 0x1e1b2a–0x1e1b95 use `nextSnap`, collecting the archival array before the current array. Native used `snap` and reversed those arrays. The native scale helpers now perform reciprocal multiplication in the same order as Mac 0x1e1374–0x1e137b and 0x1e1c90–0x1e1c9c.

**Screen alignment arguments were reversed.** Mac passes the horizontal bits first and the vertical bits second, for example 0x1e02e2–0x1e0310 and 0x1e072d–0x1e0755. The text baseline and both material paths also now preserve Mac's unusual `(fromY - toY) * fraction + fromY` calculation at 0x1e03fb–0x1e0409, 0x1e195c–0x1e196a and 0x1e1a0f–0x1e1a1d.

**HUD element preparation had incorrect sizes, colors and timer text.** Mac 0x1e0624 clears an absent label's width; 0x1e0d0c uses the correctly rounded one-third font multiplier; 0x1e0d8e uses a waypoint's declared width; and 0x1e08c6 takes the larger of font and material/clock height. Mac 0x1e07eb–0x1e0883 fades from the old color toward the new color. Native reversed that fade. Only settled positions reach the pixel-rounding block at 0x1e075a; interpolated positions remain fractional. The decoded tables at 0x33c650, 0x33c67c and 0x33c694 also show that tenths countdowns add 99 milliseconds and clock elements do not produce timer text.

**Clock assets used the wrong suffix and countdown bias.** Mac 0x1e15cd appends `needle`, and the clock-down target at 0x1e18c8 uses the raw remaining milliseconds. Native appended `_hand` and added 99 milliseconds. The 2D type bound now follows Mac's unsigned comparison at 0x1e12ce.

The alignment helpers, hidden-HUD gates and sort comparator match. The comparator's unordered float case returns zero on both sides. The sprite path's white RGB, original alpha, size fallback, 0.0043 multiplier and 0x3000 rendering flags were also compared.

## Verification and limits

Both changed engine source objects compile with the arm64 Release flags, and `cod2_macos_ded` builds with two low-priority jobs. No new tests were written. The HUD element fixes are compiled and reviewed but have not been exercised by a rendering or replay run.

Existing native handling of invalid font indices, missing font handles and null/empty string calls remains defined. Mac can leave font state uninitialized for an invalid font index; that storage behavior was not introduced into native code. The existing native player-name lookup uses the typed `bgs.clientinfo` array and rejects negative indices, matching Mac's unsigned 0–63 bound while avoiding legacy raw offsets. These are specific native storage/interface adaptations, not evidence that malformed font inputs match Mac.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-hud.noindex/`.

## Integrated batch verification

Integrated source 6b334b3 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
