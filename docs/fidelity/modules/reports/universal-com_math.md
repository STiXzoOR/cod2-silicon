# universal/com_math module reconciliation

Updated 2026-11-05.

**Current: 171/229 reconciled: 141 equal, 30 fixed, 0 intentional; 58 unchecked.**

Original pass:160/229 reconciled:136 equal,24 fixed;69 unchecked. The initial 245-row assignment had 123 reconciled entries: 82 native math definitions and 41 complete inline or alias identities compared by hand with Mac 1.3 symbol-start disassembly. Its original 22 fixed ledger rows cover 17 native functions and five aliases of the same corrections. Triage and constant checks supplemented that reading. All 486 `bytedirs` floats and all 64 bytes of `identityMatrix44` match the Mac data. The eight-case `BoxOnPlaneSide` jump table at 0x328460 was decoded.

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

**Current: 165/229 reconciled: 138 equal, 27 fixed, 0 intentional; 64 unchecked.**

Original pass:160/229 reconciled:136 equal,24 fixed;69 unchecked. This pass closes 37
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

## Retained original-address provenance and integer selectors

Original Linux addresses have now been recovered instead of relying on names in
the reconstruction. The retained 1.0 IDA database supplies function boundaries
and bytes. Masked whole-body comparisons against the original 1.3 ELF identify
all 155 math bodies with a common relocation of +0x2364; 14 identical-body
ambiguities are disambiguated by the surrounding module sequence. This maps
61 of the 69 remaining math identities to concrete original addresses. For
example, OrthographicMatrix maps 0x80a5b9c to 0x80a7f00, QuatInverse 0x80a5494 to
0x80a77f8, Vec3PackUnitVec 0x80a23c8 to 0x80a472c and TransformBoundsExpand
0x80a6f20 to 0x80a9284. These mappings supply identity and intent only, and do
not close those rows without corresponding complete Mac operations.

I_min is now fixed and I_max equal based on original caller chains and complete
consumed Mac operations. Original I_min copies at 0x805525e and 0x81293be serve
partition scoring, FX cull-plane selection and four-channel byte packing.
Original I_max copies at 0x80f292e and 0x812991c serve the weapon kick timer and
that byte packing. The original Flash caller at 0x812915e reaches byte packing
at 0x81291c0; the original FX cull wrappers reach their shared selector through
seven explicit calls. This replaces the earlier speculative name association.

Mac partition selection at 0x7800f/0x780df chooses by subtraction sign. Both
counters start at -1 and increment at most N times for positive signed N, so
their difference cannot overflow and native comparison agrees. Mac weapon
selection at 0x7cd9a–0x7cdaf first requires a positive kick timer, subtracts
pml.msec and selects zero by sign. Pmove bounds that duration to 1–200, making
the subtraction defined; the native timer operation agrees. Flash Draw's four
upper/lower selections already preserve wrapped subtraction and negation,
including the integer-indefinite conversion sentinel. The seven FX cull owners
needed the wrapped count-minus-five correction described in the FxPrimitives
report. The helper verdicts cover these original consumed operations, not a
newly invented arbitrary-argument Mac function.

I_clamp and I_sel remain unchecked: original header-copy identities were
recovered, but their additional CrashLand exceptional conversion and nested
selection contract is still incomplete. FloatIdentity and SinCos now have
original addresses but need their complete Mac owner identity. Narrow semantic
Mac scans for unit-vector packing constants and quaternion conjugation did not
establish a new complete owner and do not count as absence evidence.

Private address maps, original caller packets and exact Mac entries are retained
in recon-provenance-math4.noindex. Only the FX translation unit changed and was
manually compiled in its two configured client variants. No runtime behavior,
tests, CI, full build or replay was exercised. There are 67 unchecked math rows.

## Complete I_clamp and I_sel caller operations

The remaining original I_clamp copies are now tied to complete operations by
actual original executable calls: PM_CrashLand 0x80e30c4 calls 0x80e95f8 at
0x80e3263, while Particle UpdateRGB 0x81264a2 calls FX_ColorToBytes 0x812430c at
0x8126529, which calls 0x8129422 at 0x8124367. Mac RGB's three-channel conversion
loop at 0xa1e62–0xa1e89 already agrees with native FloatToByte, including the
wrapped upper selection followed by the lower test on the original integer.
The CrashLand correction and its conversion/branch limits are described in the
bg_pmove report. I_clamp becomes fixed.

Original I_sel copies at 0x80553d2, 0x80e9a50, 0x80f2b14 and 0x81298fc are now
linked by their actual calls to the complete I_min/I_max/I_clamp operation set:
partition scoring, weapon timer, FX culling, Flash packing, RGB packing and
CrashLand. With the FX culling and CrashLand wrapping selections corrected,
I_sel also becomes fixed. These are consumed-operation verdicts; no independent
arbitrary-argument Mac entry or new public API is claimed. This leaves 65 math
rows unchecked. Only the changed bg_pmove translation unit was manually compiled
in its four configured modes; no tests, CI, replay or runtime behavior ran.

## PPC OrthographicMatrix identity lead

The retained fat Mac binary's PPC slice contains an actual OrthographicMatrix
body at 0x2beb8. Direct original branch encodings establish the complete caller
chain: R_RenderScene 0xcdbb0 calls R_GenerateShadowCookies 0x10335c at 0xce668;
that calls R_AddShadowCookie 0x1029c4 at 0x103878, which calls the matrix helper
at 0x102e14. This is positive original Mac identity evidence, beyond the
reconstruction-only references available earlier.

The helper and all three PPC callers were retained privately and the exact
corresponding i386 R_RenderScene body at 0xc5e5c was inspected. A complete
corresponding i386 matrix operation/native owner was not established. The entry
remains unchecked; PPC supplies identity and caller intent without overriding
i386 floating behavior or justifying a disconnected native helper. No source
changed and no compilation, test, CI, replay or runtime operation ran.

The same packet records a targeted i386 executable-section trig-call inventory
for the recovered original SinCos→VectorRotate→VectorRotateAroundOrigin chain.
It did not establish a complete Mac counterpart and closes no additional row.
Private evidence: recon-math6.noindex.

## FloatIdentity provider boundary

Original1.3 FloatIdentity copies0x80553f2/0x80b933a load the incoming word as float, and original FloatNoSwap calls the latter. Exact Mac FloatReadNoSwap has matching word instructions, but its input contract is raw integer bits. No positive float-input consumer/representation adapter connects the original helper to that native API. Independent review therefore retains FloatIdentity as unchecked; no cross-ABI alias or arbitrary NaN/exception equality is asserted. The separately found FloatReadSwap signed-shift correction belongs to the q_shared provider and does not close this helper.


## FloatIdentity in the collision-plane loader — shared8

Complete consumed FloatIdentity operation identified in original CMod_LoadPlanes 0x8053d82: actual source-field ADDRESS reaches raw-word dereference helper 0x8055284 at 0x8053e54, then FloatFromBits 0x8055374 at 0x8053e5c, then FloatIdentity 0x80553f2 at 0x8055380; returned float is stored to the matching normal field. Distance repeats the same full chain at 0x8053e9d/0x8053ea5. Fresh complete Mac CMod_LoadPlanes 0x77dc2 has the corresponding raw normal load/local/MOVSS/store at 0x77e3e–0x77e4b and distance MOVL transfer at 0x77e63–0x77e6c. Native cm_load_obj.c copies those same three normal fields and distance. This actual storage adapter resolves the prior hypothetical float-input/int-input bridge: no numeric conversion, callback cast or disconnected API is invented. Equal applies to the identified Mac/native consumed field transfer, not standalone FloatIdentity ABI, original Linux FLDS/FSTPS signaling-NaN/exception behavior, malformed-BSP storage, later comparison side effects or whole loader equivalence. FloatNoSwap/FloatSwap remain separately unchecked. Independent review: review-provenance-shared8-floatidentity.md. No source changes, compilation, tests, CI, replay or runtime verification.

The prior standalone provider rejection remains valid for FloatNoSwap and FloatSwap. This closure uses the newly recovered complete original loader adapter and its actual corresponding Mac/native storage transfer. Existing plane-loader source, layout, count checks, diagnostics and classification logic are unchanged. Private evidence: `recon-provenance-shared8.noindex`.


## Complete transformed-bound union — math10

Complete transform-and-union operation identified in active Mac/native R_UpdateXModelBounds, not merely a transform or expansion fragment. Original wrapper identity 1.0 0x80a6f20 / 1.3 0x80a9284 forwards input bounds/origin/axis to complete TransformBounds 0x80a902c using a private six-float temporary, then merges all six endpoints through ExpandBounds 0x80a8eac. Exact Mac per-bone block 0xd0a91–0xd0ce4 performs the full operation: nine sign-bit/component-index endpoint selections, origin plus three ordered products for each min/max, then all six conditional accumulator updates. X merge 0xd0b21–0xd0b52, Y 0xd0be4–0xd0c15, Z 0xd0cb3–0xd0ce4 retain existing endpoints on equality/unordered input. Native r_model.c AXIS_CONTRIB plus the complete three-axis update uses the actual XBoneInfo bounds, quaternion-derived axis, bone translation and separate local bounds[6], with the same operation. This establishes the complete consumed disjoint-buffer operation; no standalone arbitrary-alias API, Linux float authority, whole renderer recertification or closure from zero-address ExpandRotatedBounds.eh is claimed. The final entity GetRotatedBounds call is separate. The existing component-offset repair predates reconciliation (83053c55, Oct3, ancestor of renderer audit555ba3e3), and the current primary Mac owner is already equal; no new defect/owner override. Independent review accepted the complete operation and classification. No source changes, compilation, tests, CI, replay or runtime verification; existing adaptations preserved.

The original 1.3 scan of the remaining 61 emitted math bodies found six direct calls, all inside unresolved math-helper chains, and no new non-EH absolute references. This bounded result closes nothing. The new finding instead follows the active rendering bounds provider into a complete per-bone transform and merge. Original Vec3NormalizeByMax was also read through its zero provider: it writes three zeroes and divides by the maximum, while Mac/native ColorNormalize writes white at zero and uses reciprocal scaling. That different full contract remains unchecked. Color-byte candidates likewise have different byte order/rounding interfaces. Sixty-three math rows remain unresolved. Private evidence: `recon-provenance-math10.noindex`.


## Complete camera in-place multiplication — math11

Complete consumed in-place left multiplication identified in CG_PerturbCamera, inheriting its existing contiguous-basis repair. Original full MatrixMultiplyInPlace identity 1.0 0x80a379a / 1.3 0x80a5afe retains six output floats temporarily, writes the final row and copies the first two rows back, establishing inout=left*old-inout. Exact Mac camera 0x1d6ca6–0x1d6cbd snapshots all nine view-axis floats through AxisCopy 0x2c320 into a separate local; the SAME saved view-axis pointer in EBX is then the MatrixMultiply output at 0x1d6ccd, with the snapshot as right input and the complete contiguous perturb basis as left input. Full Mac MatrixMultiply 0x29baa–0x29d6d and native provider preserve all nine ordered dot products. Native CG_PerturbCamera implements that actual snapshot/overwrite sequence, not merely a hypothetical use of a three-buffer provider. Existing correction 3ef61de8 made the native left basis one contiguous 3x3 object, matching Mac stack rows -0x3c/-0x30/-0x24 instead of reading unrelated locals after a three-float vector. The primary owner is already fixed; this alias inherits that correction, with no new defect, source change or owner override. Scope is the complete consumed operation with separate left basis and saved right input; no arbitrary overlapping-left/right API, exact original temporary-store timing, whole-camera recertification or Linux x87/FP-environment equivalence is claimed. Existing correction is compiled and reviewed, not exercised by this operation audit. Independent review: review-provenance-math11-inplace.md. No new compilation, tests, CI, replays or runtime exercise.

The earlier rejection of bare MatrixMultiply/MatrixMultiply43 providers remains valid. The new witness supplies the missing actual input snapshot and same-object output, with all nine values and the repaired left-basis storage established. MatrixMultiply34 and MatrixTransformVector43Equals retain their separate layout/input/output gaps. Sixty-two math rows remain unchecked. Private evidence: `recon-provenance-math11.noindex`.

## Distinct live sampler owners (provenance-math12)

Four remaining sampler rows received narrower blockers, with no closure or source change. The original `SphericalToCartesian` body at 0x80aa9c8 passes the same angle to both complete sine/cosine providers: its three outputs use radius times cosine squared, sine times cosine, and sine. The second angle is not missing from the reconstruction. The previously examined FX sphere owner uses independent angles and separate width/height factors, so it still does not identify that complete operation. Linux supplies original identity and intent here, not native floating-point authority.

Two other live sampling owners were read from fresh exact Mac bodies. `Bullet_Endpos` at 0x1c8cb2 uses a linear random radius for its two transverse offsets, whereas the original uniform-circle helper derives a square-root radius. The complete `Cloud` constructor at 0xa98e6 draws three signed components, accepts squared length in [0.01,1] within four attempts, and normalizes or falls back to (1,0,0). It does not perform either original two-deviate sphere/hemisphere construction. Corresponding native owners preserve these Mac operations. Similar distribution intent does not establish complete helper identity.

All four rows remain unchecked; module counts remain 139 equal, 28 fixed and 62 unchecked. These are bounded candidate eliminations, not proof that no other owner exists. Private exact-body evidence is retained in `recon-provenance-math12.noindex`. No tests, CI, compilation, replay or runtime verification was performed.

## Further complete-operation candidates (provenance-math13)

Seven rows remain unchecked after distinct candidate checks. The original `AxisFromForwardYaw` builder was compared with the complete mark-basis sequence at Mac 0x17c06b–0x17c0ac. Marks always rotates, stores cross before rotated side, and passes that packed order to `CL_MarkFragments`; the original wrapper skips signed-zero yaw and uses the opposite two-axis order. The current native mark layout is preserved. Without an original use bridge, this is a different complete builder rather than a justified wrapper identity.

The full original constant-step trackers were compared with Mac `PM_UpdatePronePitch` at 0x6cafe. Prone pitch uses quantized `AngleDelta`, exact-zero gating, a signed frame-time step and accurate final wrapping. The original pair has an explicit 0.001 tolerance, absolute-step clamp and target-relative wrapping chain. Current native code preserves the Mac tracker; this candidate does not establish either missing helper.

A semantic sign-operation scan found a concrete three-negation candidate in Mac `CDirect3DDevice::ValidateLighting` at 0xe784. Reading the complete body shows literal zero in the fourth component at 0xe87f, rather than the fourth-word preservation required by original `QuatInverse`. The native direction construction agrees. Additional integer-XOR and packed-mask scans are retained as bounded leads, not absence proof. A packed-vector constant route instead found scoreboard text positioning at 0x1c72ad/0x1c74e5; no complete three-field pack/unpack operation was established. Finally, complete original IDB bodies named `VectorSubtract417` and `VectorSubtract412` each process only three components. Their suffixes do not establish the four-component header helper's identity.

Module counts remain 139 equal, 28 fixed and 62 unchecked. No source changes, new defects, tests, CI, compilation, replays or runtime verification. Exact bodies, bounded scan scripts/results and original-byte evidence are retained in `recon-provenance-math13.noindex`.

## Final scalar trigonometry candidate pass (provenance-math14)

Full original bytes establish double input and two double stores for `SinCos`; `SinCosDegrees` instead adds 360 once for negative input, uses literal outputs at exact 0/90/180/270, and narrows its fallback angle to a float pair. A bounded primary Mac direct-import inventory found no owner using both double sine and cosine. Complete `AngleVectors`/`AnglesToAxis` use float pairs and vector products without that quadrant table. No hypothetical two-call adapter or absence-based closure was adopted.

The single-axis quaternion bodies all use the same positive half-degree constant, two literal positive-zero components and the sine/cosine pair in the active component and w. The complete Mac axis-angle provider multiplies every supplied axis component by sine; a hypothetical unit-axis input does not preserve literal zero fields for signed-zero or NaN results. The actual DObj setter chain computes the complete three-angle Euler product, with an identity-only null-pointer branch. These remain distinct operations.

Two concrete double-sine consumers also fail the full `SinSquaredDeg` contract: stance opacity at 0x18bf60 converts the sine to float and takes its absolute value; complete `FresnelTerm` squares a ratio of sines in a larger reflectance calculation. All six entries remain unchecked, with original widths/constants and exact candidate addresses recorded privately in `recon-provenance-math14.noindex`. Math remains 139 equal, 28 fixed and 62 unchecked. No source changes, new defects, compilation, tests, CI, replay or runtime verification.

## In-place affine owner follow-up (provenance-inplace17)

MatrixTransformVector43Equals remains unchecked. Its complete original body at
0x80a759e–0x80a766e (original 1.0 identity 0x80a523a) computes three affine
coordinates, saves the first two, then writes z/x/y back into the original point.
The actual overwrite is part of this helper's operation.

Fresh exact Mac Tail::CalcNewEndpoint 0xa130a and Cylinder::Update 0xa5780
pass a separate stack point to this+0x9c. Active native code preserves that
separation; apparent same-endpoint calls exist only in inactive legacy branches.
FX_CalcOriginAndAxis 0x59d67 passes its local world point at stack-0x48 to the
caller output through the inverse orientation provider. The native copy-back
there does not establish an affine-matrix input overwrite. R_SkinXSurfaceSkinned
0xd1d6c–0xd1e13 computes all three affine coordinates from the vertex array into
distinct renderer output storage. Temporary results alone do not prove aliasing.
These concrete operations do not close the remaining row, and they do not
exclude a different undiscovered owner. No source change or new defect.

Private exact packets are in recon-provenance-inplace17.noindex. No tests, CI,
compilation, replay or runtime exercise were performed.

## Actual projected two-coordinate bounds (bounds2d15)

`AddPointToBounds2D` is equal for the complete disjoint point-accumulation operation in Mac `R_VisitPortals`. Original 1.0 0x80a66f4 and 1.3 0x80a8a58 establish all four strict min/max updates for one two-coordinate point. Primary Mac 0xf219b–0xf21d5 performs exactly those four updates for actual projected portal points; old bounds are the second MINSS/MAXSS operands, preserving old values on ties (including signed zero) and unordered candidates. Native `r_dpvs.c` retains the same four strict comparisons. No third coordinate, reset or extra result is attributed to this operation. The actual loop carries the four bounds and the bevel path consumes both minima/maxima in four inverse-projected rectangle corners.

This is a complete observed disjoint consumer, not a hypothetical 3D prefix or exported arbitrary-aliasing API. Quiet unordered projected candidates retain the prior bounds; initial finite bounds cannot become NaN through these updates. Signaling/exception timing, concurrent observation and literal original inline ancestry are not certified. The separate area-consumer correction in portalarea15 changes the later use of maxY, not these four already-correct updates, and does not change this equal verdict.

The other three helpers remain unchecked. `ClearBounds2D` original 0x80a892c resets both coordinates to +/-131072, whereas the actual screen owner initializes +/-1 and uses a distinct near-clip viewport reset. `ExpandBounds2D` original 0x80a8f96 accepts independent input minimum/maximum vectors; using the same point for both ends is not that full operation. `BoundsOverlap2D` original 0x80a8ce2 returns the result of four strict rectangle-separation comparisons, accepting touching/unordered boundaries. The inspected portal owner instead clips polygons through side planes and computes projected area. Integer scissor APIs supply no demonstrated replacement for that float rectangle contract. These are bounded candidate limits, not absence-based conclusions.

Independent review: units/review-provenance-bounds2d15.md. One equal closure, three refined unchecked rows, zero new defects in these helpers and no source changes in this packet. Original complete bodies, fresh Mac owner and native state/consumer paths were reviewed. No compilation, tests, CI, replay or runtime verification for this packet; separate portalarea15 compilation does not add exercise coverage. Private evidence: recon-provenance-bounds2d15.noindex.

## Complete segment-distance operation (provenance-distance18)

DistSqToSegment is fixed through the actual DObjTraceline sphere broad phase.
The complete original helper at 0x80a414e and all called providers establish
point projection, endpoint clamping and three-component squared distance. Mac
0x76600 produces the real endpoint delta and reciprocal squared length, transforms
the actual bone collision center, and selects all three end/start/interior
residuals before the squared sum feeds radius rejection at 0x76ba4.

This complete endpoint-defined operation identifies the original segment intent;
it is not a hypothetical normalized-direction adapter. Its newly found unordered
selector is corrected in the DObj owner. Ordered zero-length segments return
before the operation, so no standalone zero-length helper result is certified.
Arbitrary nonunit directions, negative lengths, exported ABI, Linux x87 equality,
NaN payloads/FP flags and whole-owner equivalence are also outside this verdict.

PointToLineDistSq remains unchecked: the complete triangle-edge residual at
0x66925–0x669c9 uses a serialized/rescaled direction; the loader's copied edge
fields at 0x79503–0x795b5 do not establish an actual endpoint-to-axis bridge.
ProjectPointOntoLine remains unchecked: original 0x80a556a copies start on ordered
zero length, whereas CG_WhizbySound gates by 64-unit endpoint margins before its
projection and later shifts the sound position by 16 units. FX visibility also
has its own minimum-length and centered-segment gates. No partial dot or invented
input adapter closes either row.

Independent mapping and source reviews are review-provenance-distance18-segment.md
and review-provenance-distance18-selector.md. Private exact evidence is in
recon-provenance-distance18.noindex. Only changed dobj.c was manually compiled in
four existing modes; no tests, CI, full build, replay or runtime exercise.

## Complete fixed-alpha color word (provenance-packing20)

ColorBytesFromRGB is equal through the actual team-color chain. Original
0x80a810c establishes RGB conversion, fixed fourth byte255 and packed-word
intent. Mac CL_UpdateColor 0x17b88a obtains bounded byte-derived team RGB and
sets each alpha to1. Its real export slot0x10c resolves to RB_UpdateColor
0xd4c92, which calls the complete four-slot converter0xd4bd8. All channels
retain their byte offsets; multiply255/add0.5/floor/CVTT is the authoritative
Mac rounding sequence. The persistent words are read and copied whole by
RB_LookupColor at0xd4684/0xd4692. Native preserves that complete chain.

This is a positive fixed-alpha adapter and full-word consumer, distinct from
the earlier ARGB and isolated scalar candidates. It certifies the actual bounded
consumed operation, not arbitrary float inputs, an exported return-register ABI,
Linux x87 rounding, overlap, or subsequent use of alpha. No exceptional-source
defect or runtime occurrence is claimed. ColorBytesFromRGBA remains unchecked:
the observed caller fixes alpha1, so it does not establish a variable fourth
input role even though the complete provider reads all four slots. Independent
review accepted RGB only in review-provenance-packing20-color.md.

The signed10-bit vector helpers remain unchecked. Complete original bodies
confirm511 scaling, mask0x3ff, shifts10/20 and signed22-bit shifts on unpack.
A new bounded instruction scan did not locate a complete engine format owner.
Exact XSurfaceTransfer 0xf9b2c and its Dx7 companion instead copy all three
normal floatwords into renderer storage; this concrete format does not establish
a packed-word adapter. ClampShort also remains unchecked: exact32767 candidates
ValidationProc and GetMenuBuffer use text-selection markers or file-size
rejection, not signed saturation. External library candidates are not reused as
engine ownership evidence. These bounded checks do not prove semantic absence.

Evidence is retained in recon-provenance-packing20.noindex. No source edits,
compilation, tests, CI, replay or runtime exercise were performed.

## Geometry consumer boundaries (provenance-geometry21)

Five entries remain unchecked after reading their complete original bodies and
new concrete Mac/native candidates. No source defect or closure is claimed.

VectorRotate at original0x80aa82a is sequential X/Y/Z rotation with a separate
ordered-zero skip for each component; unordered inputs enter that rotation.
VectorRotateAroundOrigin at0x80aa974 subtracts the pivot, invokes that operation,
and adds the pivot to all three outputs. Mac G_TryPushingEntity0x1ba63c provides
a real complete pivot rotation: movement, relative point, all three basis
products, then rotated-minus-relative deltas before capsule use. Its actual
AngleVectors provider0x2d938 uses pitch/yaw/roll, however: component0 changes
the X/Z plane rather than the original helper's Y/Z plane. No actual convention
adapter was established. CM_TransformedPointContents0x693ce instead applies an
inverse model basis after a single all-zero gate and consumes a local point.
Neither candidate proves the missing original input-role mapping; a hypothetical
matrix or three single-axis calls would not supply it.

IntersectPlanes at original0x80a97e0 consumes three normal/distance planes,
checks the determinant and produces three coordinates plus success. Exact
R_ChopPortalWinding0xee7fe clips existing polygon endpoints against one plane;
its three-coordinate interpolation0xee94d–0xee9d2 does not supply a three-plane
solve or determinant failure. MatrixInverse0x2a79e emits nine coefficients
unconditionally, with no plane-distance inputs or success result. A bounded
instruction scan of ordinary and coalesced text for0.001 float/double references
found no new complete candidate. This is discovery evidence, not proof of
semantic absence, and no plane-to-matrix adapter is assumed.

For PlaneNormalFromPoints and PlaneFromPoints, new mark and occluder candidates
add concrete producer evidence beyond the earlier side-normal search.
R_MarkFragments0xf4c8d–0xf4d24 forms two indexed triangle edges and their cross,
but consumes the unnormalized magnitude in facing tests. Its side-volume path
uses an edge and an extrusion axis. R_VisitPortalsForCell's perspective occluder
branch0xf096f–0xf0a13 forms both endpoint-eye differences, crosses them, then
joins normalization0xf0864 and stores additive negative-dot distance0xf089b
plus side metadata. It discards the normalization return and always writes the
fourth coefficient. Original PlaneFromPoints0x80aa04a returns0 for ordered-zero
normal length before writing its fourth slot; no actual original caller was
recovered that establishes a compatible narrower consumed contract.
PlaneNormalFromPoints0x80a54f0 additionally normalizes both differences before
crossing them, a provider chain these candidates do not establish. The unresolved
operation contracts, rather than Linux/Mac arithmetic grouping, prevent closure.

Focused packet: provenance-geometry21. Private exact bodies, discovery inventory,
and source hashes: recon-provenance-geometry21.noindex. No source edits,
compilation, tests, CI, replay or runtime exercise. Counts remain141equal,
29fixed and59unchecked; all previously settled evidence is preserved.

## Actual point-query bounds adapter (provenance-pointbounds22)

PointInBounds is fixed through the existing unordered-comparison repair in
CM_AreaEntities_r. Original0x80a8aee establishes the complete six-comparison
inclusive point/min/max predicate and membership result. Mac SV_PointContents
0x1bfb3c–0x1bfb46 passes the same point pointer as both bounds to CM_AreaEntities;
the wrapper preserves both pointers. The actual entity predicate0xaf6d2–0xaf729
therefore checks all three point coordinates against entity minima/maxima, in
X-min/X-max,Y-min/Y-max,Z-min/Z-max order. Each UCOMISS/JA rejects only ordered
greater-than; touching and unordered comparisons continue. Success reaches the
separate capacity gate and actual entity-list store/count0xaf763/0xaf766.

Native preserves that full adapter and predicate. Existing repair a826bfe0
replaced legacy <= tests with negated greater-than predicates; this closure
inherits that fix without a new source change or defect. It is a complete
consumed point-membership operation, unlike the earlier rejected strict-interior
CM_PointContents leaf candidate. It does not certify whole tree traversal,
capacity/query results, subsequent collision, malformed-point enumeration,
standalone helper ABI or floating exception/payload behavior. In particular,
unordered point values can affect earlier traversal; no runtime occurrence is
claimed. PointInBounds2D remains outside this packet.

Independent review: review-provenance-pointbounds22.md. Exact original and Mac
caller/wrapper/provider bodies and source hashes are retained in
recon-provenance-pointbounds22.noindex. Existing provider verification remains
historical; no compilation, tests, CI, full build, replay or runtime was run.
