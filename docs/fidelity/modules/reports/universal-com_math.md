# universal/com_math module reconciliation

Updated 2026-11-05.

**160/229 reconciled: 136 equal, 24 fixed; 69 unchecked.** The initial 245-row assignment had 123 reconciled entries: 82 native math definitions and 41 complete inline or alias identities compared by hand with Mac 1.3 symbol-start disassembly. Its original 22 fixed ledger rows cover 17 native functions and five aliases of the same corrections. Triage and constant checks supplemented that reading. All 486 `bytedirs` floats and all 64 bytes of `identityMatrix44` match the Mac data. The eight-case `BoxOnPlaneSide` jump table at 0x328460 was decoded.

## Corrections

`RotatePointAroundVector` at 0x2d65c used the opposite perpendicular basis. Mac 0x2d77a-0x2d7c4 forms `vr` cross `dir`; native formed `dir` cross `vr`, reversing the rotation direction.

`MatrixInverse44` at 0x2a9da now subtracts a complete three-term sum from another three-term sum for every cofactor. The former sequential subtraction changed float rounding. `MatrixMultiply43` at 0x2a02c now writes rotation columns in Mac order before translation, preserving the original behavior when storage overlaps. `MatrixForViewer` at 0x2b87e computes the first translation from the already-negated axis, including its signed-zero behavior.

`GetRotatedBounds` at 0x2c674 now selects endpoints by the axis sign bit, including negative zero. Independent review caught an unsupported GCC sign-bit builtin on MSVC; the final implementation copies the float representation into `uint32_t` and masks bit 31. The corrected arm64 dedicated target rebuilt successfully; no MSVC build was run. `YawToAxis` at 0x2d5ba retains subtraction from positive zero and addition to positive zero for its second axis.

The unordered comparison paths now match Mac in `DiffTrack` (0x2c220), `BoxDistSqrdExceeds` (0x2c540), `ExpandBoundsToWidth` (0x2c452), `WindingContainsCoplanarPoint` (0x2d0ee), and `AxisToAngles` (0x2dcc2). In particular the width selection uses a rounded subtraction, and the box-distance minimum retains the same unordered/tie operand.

The packed angle conversion in `AngleMod` (0x2b99c), `AngleNormalize360` (0x2ba88), `AngleNormalize180` (0x2cd68), `AngleDelta` (0x2cdac), and `DiffTrackAngle` (0x2ce40) now handles Mac `CVTTSS2SI` invalid inputs explicitly. Its integer-indefinite result masks to zero; a direct C float-to-int conversion does not preserve that contract on arm64. `irand` at 0x2c0aa now expresses the range subtraction and result addition as 32-bit wrapping operations, matching the Mac integer instructions without signed C overflow.

## Verification and limits

All changes are under `COD2_X64`. Selecting the legacy branches produces exactly the starting source text, byte for byte. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. Existing sway, FX primitive and water fixtures were inspected; no tests were added or changed. The corrections are compiled and reviewed, not exercised. Full CI and replays remain with the integration owner.

The 122 unchecked entries comprise 106 Linux-only helper rows without an established complete Mac inline identity, and 16 unrelated caller functions attributed to this header. The inline pass established complete copy, comparison, dot, length, scale, clamp, transform and sign-predicate operations in exact caller ranges. The Dvar_ValuesEqual dispatch table at 0x328b18 was decoded to identify the two-, three- and four-component comparisons and byte comparison. Similar arithmetic alone was not treated as proof of a whole caller. Remaining helper evidence names searched reference callers, or records an absent native body/use contract. For example, AxisClear and AxisIdentity have only unreferenced reference definitions; FloatAsInt has a known sign-only use but no complete raw-integer-return identity. Q_rint and I_fround have a located floor/conversion block, but exceptional C conversion behavior in the unrelated native caller remains unverified. The private `universal-math.routing.json` maps all misattributed callers to their actual source and module for later ownership correction. No existing reconciled verdict or platform decision was changed.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-math.noindex/`.

## Ownership correction

16 unchecked whole-caller rows were moved to their actual engine modules after integration. Original Mac attribution, IDs and unchecked verdicts are preserved. The preceding audit totals describe the original assignment. After that ownership correction, before the 2026-11-05 follow-up, this module held 229 rows: 101 equal, 22 fixed, 0 intentional, 106 unchecked. The destination owners must audit each complete caller; matching a shared inline helper does not certify it.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.

## Remaining-helper follow-up — 2026-11-05

**160/229 reconciled: 136 equal, 24 fixed; 69 unchecked.** This pass closes 37
of the 106 remaining Linux-only helper rows: 35 equal and two fixed. The original
123 reconciled statuses/evidence remain unchanged. Linux supplied helper names;
Mac symbol-start disassembly supplied behavior. Every new closure accounts for
the complete helper operation, including every component, branch and result.
It does not certify the unrelated caller around an inline operation.

Two corrections were required in actual native owners. `FastCeil` in the HUD
timer/clock code used a C float-to-int cast. Mac 0x1b3d4f, 0x1b3f03 and 0x1b3f39
use `CVTTSS2SI`, whose invalid result is `INT_MIN`. A connected native helper now
preserves that result after `ceilf` at all three uses. `BoundsOverlap` in
`CM_AreaEntities_r` used six `<=` tests, rejecting unordered pairs; Mac
0xaf6d2–0xaf729 rejects only strictly separated pairs with `UCOMISS/JA`. The
native predicates now preserve that unordered-inclusive behavior and finite
axis/order behavior. Both corrections are guarded by `COD2_X64`. Their complete
owner rows receive explicit parent-approved evidence overrides.

The new identity evidence includes:

- `FloatAsInt` is the complete native `FloatWriteNoSwap` operation, Mac
  0x28590–0x28597, returning the entire float word. This resolves the previous
  sign-only evidence gap. `FloatIdentity` remains distinct and unchecked.
- `Q_rint`/`I_fround`, `Q_round` and four-component byte packing use the complete
  `Flash::Draw` sequence at 0xa0e83–0xa0f71. Later native FX conversion helpers
  already preserve invalid integer conversion and the wrapping clamp paths.
- `AxisClear`/`AxisIdentity` match the full nine-word camera identity block at
  0x1d4826–0x1d4867; the decoded cubemap table selects it for case 4.
  `Vec3ScaleToUnitMax` is the complete `ColorNormalize` operation at
  0x2c7a6–0x2c82e, including output ones on zero and its returned maximum.
- Paired float sine/cosine, reciprocal square root, float sign, integer sign,
  side predicate, three-operand float clamp and accumulated plane-sign bits
  were read in their exact owners. Mac's comparison-based signs and unordered
  side result take precedence over the Linux header expressions.
- Complete two-/three-/four-component copies, sets, clears, sums, differences,
  averages, scales, multiply-adds, interpolation, negation and squared length
  were established in Dvar, collision, animation, camera, item and movement
  owners. Private ledger evidence names each exact range and native owner.

The 69 unchecked rows retain real identity or input-contract gaps. New leads
were evaluated without converting an absent symbol or partial operator into a
closure. In particular, the partition integer minimum uses sign-only selection
while native brush counts are bounded; the generic integer clamp has a located
wrapping crash-land block but no established complete exceptional caller/input
contract. Separate-output normalizers are not in-place normalizers, unsigned
angle routines do not prove signed angles, matrix APIs do not establish the
special alias/store contracts, and the FX random sphere sampler does not prove
uniform-deviate or golden-angle samplers. Existing float-bit, plane snapping,
projection, quaternion and remaining geometry gaps stay explicit. No unused
standalone helper was added.

Both changed translation units compile with the configured arm64 Release
client and dedicated commands in stock and CoD2x modes (eight objects). All ten
existing HUD module groups pass both modes under ASan/UBSan; the existing
production `world_links` fixture passes both modes under ASan. No tests were
added or changed. Five inactive preprocessing configurations match the base;
`unifdef -UCOD2_X64` output is byte-identical for both sources, and neither uses
`__LINE__`. These are source-preservation checks, not i386 byte parity.

The exceptional conversion and unordered-overlap corrections are compiled and
reviewed, not specifically exercised. Full links, CI and replays remain with
the integration owner. Parent/fresh source review is pending at this worker
commit. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-math-remaining.noindex/`.

## Fresh caller-review corrections — 2026-11-05

Independent review confirmed the 37 helper identities and the original HUD
ceiling/area-overlap corrections. It also found four complete-caller defects
around the helper scopes: unordered child/prune/parallel gates in
`CM_TraceThroughTree`, the cylinder vertical gate in `SV_EntityContact`,
start-solid horizontal normalization in `DObjTraceline`, and compass range/alpha
selection in `CG_DrawCompassFriendlies`. Their guarded repairs and exact Mac
ranges are recorded in the respective owner reports. Three owner rows change
from equal to fixed; the compass row remains fixed. Original evidence is
retained and appended through explicit parent-approved overrides. The math
module's 136 equal, 24 fixed and 69 unchecked totals are unchanged.

Four additional translation units compile with the actual stock/CoD2x Release
commands: 14 client/dedicated objects where applicable. All five existing
collision suites, the existing server-contact fixture and the existing xanim
trace case pass both modes under ASan/UBSan. No existing fixture executes the
full compass-friendlies caller, and no new test was added. Exceptional numeric
paths remain compiled and reviewed only. All six changed source files preserve
five inactive configurations and byte-identical OFF text, with no `__LINE__`
uses. Final source is frozen for independent reread; full links, CI and replays
remain with the integration owner. The historical point-triangle NaN-`s` gap
in `cm_mesh.c` is explicitly outside these repairs and was flagged to the
parent with its current ledger owners.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.

## Remaining identity search — 2026-11-05

The 69 unresolved rows remain unchecked. Two additional concrete candidates
were examined after reading the earlier helper and fast-math evidence:

- `ExpandBoundsByDelta`: Mac `SV_SightTraceToEntity` at 0x1bf986–0x1bf9d1
  and 0x1bfab2–0x1bfad5 builds fresh swept bounds from start/end positions,
  supplied extents and one-unit padding. The native server owner preserves
  that structure. It does not identify an in-place signed-delta update of
  existing minima/maxima.
- `OrthographicMatrix`: the native device's 2D matrices have top-left origin,
  negative Y, fixed negative Z and XY translation. The exact Mac projection
  conversion at 0x254f6–0x2562c modifies an existing matrix and includes
  device-dependent offsets. Neither establishes the complete centered
  width/height/depth matrix initializer.

These are rejected candidates, not new helper closures or whole-caller
verification. No source defect was established. No source changed and no
compilation, tests, CI or replay ran. Private exact-entry evidence is retained
in `CoD2-native-decomp/recon-identity-math3.noindex`. The 23 outstanding
`q_shared` identities also retain their prior verdicts; review of the previous
consumer investigations supplied no new complete-operation lead.
