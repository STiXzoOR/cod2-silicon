# game/g_combat_mp module reconciliation

Updated 2026-11-03; implementation baseline `eccf71a`.

Complete function comparison: **13/13 — 10 equal, 2 fixed, 1 intentional**.
Build, review and integrated replay acceptance are complete within the limits below.
Mac 1.3 is authoritative; Linux is read-only reference. No reference source or
binary payload is added to Git. Equality assumes initialized globals, valid
entity/weapon/hit-location indices, normal finite geometry, defined-range damage
arithmetic and stable entity/client slots. Native arithmetic disables contraction.

## CanDamage (equal)

All 856 Mac bytes compared. Client midpoint/half-height/right-vector construction and five ordered origin-to-target traces match, as do boolean hit count and scalar DIVSS by exact3.0 at0x1c4101. Nonclient bounds midpoint/XY corners use reversed trace direction and first-hit early return. Mask0x802891, target number and every binary32 constant/operation match. Trace-return equivalence relies on actual boolean SV_TracePassed contract.

## G_Damage (fixed)

All 838 Mac bytes at 0x1c4272-0x1c45b7 compared. Restores explicit health reload after damage notification (0x1c4407), before alive/dead selection and lower clamp. Three production callback-contract regressions fail baseline and pass fixed stock/CoD2x; normal controls pass. Immediate live script mutation during VM_Notify is not established. Damage gates, world fallbacks, normalization, flags, minimum/demigod adjustment, debug output, subtraction, notifications and seven-argument pain/nine-argument die dispatch otherwise match.

## G_DamageClient (equal)

Complete inline client path 0x1c427e-0x1c4341 and selection tails compared: takedamage/noclip/ufo/connected gates, inflictor then attacker weapon precedence, binary32 multiplication and integer truncation, nonpositive rejection and all eleven script arguments match. Maps native client branch and G_IsPlayerDamageable.

## G_GetHitLocationIndexFromString (equal)

Complete 37-byte function 0x1c3ef0-0x1c3f14 compared: compare low 16 bits of argument against 19 entries in order, return first match or zero. Native cast reproduces original unsigned-short argument narrowing. Names from Mac pointer table 0x34d1e0 match all 19 native names.

## G_GetHitLocationString (equal)

Complete 16-byte function 0x1c3ee0-0x1c3eef compared: index into 19-entry script-string table and zero-extend 16-bit result. Valid hit-location enum domain matches callers.

## G_GetWeaponHitLocationMultiplier (equal)

Complete inline path 0x1c42d8-0x1c42ec,0x1c446f-0x1c4492,0x1c4595-0x1c45a1 maps native G_GetHitLocDamageMult: zero weapon, null definition or nonbullet type uses global table; bullet type0 uses weapon location multipliers. Existing corrected predicate matches.

## G_HitLocStrcpy (equal)

Complete 26-byte body 0x1c3c64-0x1c3c7d forwards destination/source to strcpy. Reachable callback buffers and lengths retain parser contract.

## G_IndexForMeansOfDeath (equal)

Complete 77-byte function 0x1c3e92-0x1c3ede compared: scan 15 names in exact binary table order, case-insensitive comparison, first index, then identical unknown-name diagnostic and zero. Direct Mac pointer-table extraction at 0x34d160 verified all 15 literals.

## G_NullStub (intentional)

Linux-only unknown-original-name empty helper is explicitly unreferenced; its entire source tree contains only the definition. No corresponding Mac export/STABS or native reference was found. Intentionally omit this no-observable-behavior Linux artifact while retaining its ledger row. This is not a claim of Mac equality or exhaustive identification of anonymous code.

## G_ParseHitLocDmgTable (fixed)

Complete 529-byte body 0x1c3c7e-0x1c3e8e compared. Native correction restores leading control byte 0x15 on all five fatal error strings verified directly at Mac 0x2e2308,0x2e233c,0x2e2370,0x2e23a8,0x2e23d4. All 19 initial float defaults, field offsets/types, script-name allocations, gun-zero override, file path/mode, signed length gates, 11-byte header including NUL, reads/termination/close, validation and parser callback/arguments match. Native descriptor pointer stride adapts LP64 while float offsets remain four bytes. File validity and fatal nonreturning error contract assumed, as on Mac; no unrelated malformed-file policy change.

## G_RadiusDamage (equal)

All 1220 Mac bytes compared. Null attacker/radius clamp, sqrt2 query bounds,1024 entity list, ignore/damageability gates, brush AABB/nonbrush distance, strict radius/global client-ignore tests and interpolation order match. Visible and occluded trace/fraction/strict0.2-radius paths, accuracy accumulation, directions+24Z, scales, truncation and all damage arguments match. Constants decoded directly from Mach-O; actual native flags disable contraction and strict aliasing.

## LookAtKiller (equal)

Complete inline Mac blocks0x1c4c60-0x1c4ce4,0x1c4e33-0x1c4e8e,0x1c4e97-0x1c4eae match: nonself attacker takes precedence over nonself inflictor; origin difference,vectoyaw,truncation to stats1,otherwise current yaw. Redundant pure vectoyaw call retained; no separate native function is missing.

## player_die (equal)

All 1079 Mac bytes 0x1c4a7c-0x1c4eb2 compared. DObj/movement gates,bgs pointer,turret owner/weapon attribution,death notice,ordered grenade RNG/constants/weapon-fuse arguments,dead movement and animation,killed callback ten arguments,spectator score selection,death orientation,viewangles,loopSound,unlink/bounds/relink,health and handler match. Existing native repairs remain correct. Cached maxclients is stable across scoreboard calls; entity/client slot lifetimes are stable across script callbacks.

## Verification and limits

The extended complete-production-TU damage fixture passes all nine cases in
stock and CoD2x under ASan/UBSan with assertions enabled. All three new health
notification cases fail on the baseline, while six existing controls pass.
These tests verify the explicit callback-boundary contract; immediate script
mutation during the real notification path has not been established.

The new table fixture passes six cases in each build. All five exact diagnostic
checks fail on the baseline and pass after restoring byte 0x15. The success
control validates field descriptors, defaults, parser callback, script-name
roundtrips and means-of-death lookup. It passes before and after the correction.

Machine-local evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-combat-20261103.noindex/`.
Whole-body notes, baseline/fixed fixture logs, build/ABI results and the batch
review/replay artifacts are retained there. Original-server evidence and the
comparison oracle remain unchanged.

Both stock and CoD2x client and dedicated builds pass. Scoped ABI audits of the
changed translation unit pass in both modes. Both source-specific legacy checks
pass all ten inactive configurations against `eccf71a`. No public signatures or
layouts changed; no i386 binary identity is claimed.

The one independent local review checked all nine exports, inline helper
mappings, corrections and fixture evidence; it returned no findings. No external
review, extra reviewer agents or per-fix reviews were used.

Both 2,000-record, two-client native replays match the preceding accepted native
trace exactly in state hashes and all player fields, and match each other. The
command SHA, seed and IWD set agree with the validated original trace. Original
differences remain unchanged: raw velocity at frame 102, quantized state at 116,
movement flags at 652, view angles at 776 and movement time at 1803. Entity
counts, movement type, health, weapon and weapon state match throughout. This
batch does not establish whole-project parity.
