# universal/com_math module reconciliation

Updated 2026-11-04.

**123/245 reconciled: 101 equal, 22 fixed; 122 unchecked.** The 82 native math definitions and 41 complete inline or alias identities were compared by hand with Mac 1.3 symbol-start disassembly. The 22 fixed ledger rows cover 17 native functions and five aliases of the same corrections. Triage and constant checks supplemented that reading. All 486 `bytedirs` floats and all 64 bytes of `identityMatrix44` match the Mac data. The eight-case `BoxOnPlaneSide` jump table at 0x328460 was decoded.

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

16 unchecked whole-caller rows were moved to their actual engine modules after integration. Original Mac attribution, IDs and unchecked verdicts are preserved. The preceding audit totals describe the original assignment. This module now holds 229 rows: 101 equal, 22 fixed, 0 intentional, 106 unchecked. The destination owners must audit each complete caller; matching a shared inline helper does not certify it.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
