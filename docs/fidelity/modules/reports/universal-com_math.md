# universal/com_math module reconciliation

Updated 2026-11-04.

**123/245 reconciled: 101 equal, 22 fixed; 122 unchecked.** The 82 native math definitions and 41 complete inline or alias identities were compared by hand with Mac 1.3 symbol-start disassembly. The 22 fixed ledger rows cover 17 native functions and five aliases of the same corrections. Triage and constant checks supplemented that reading. All 486 `bytedirs` floats and all 64 bytes of `identityMatrix44` match the Mac data. The eight-case `BoxOnPlaneSide` jump table at 0x328460 was decoded.

## Corrections

`RotatePointAroundVector` at 0x2d65c used the opposite perpendicular basis. Mac 0x2d77a-0x2d7c4 forms `vr` cross `dir`; native formed `dir` cross `vr`, reversing the rotation direction.

`MatrixInverse44` at 0x2a9da now subtracts a complete three-term sum from another three-term sum for every cofactor. The former sequential subtraction changed float rounding. `MatrixMultiply43` at 0x2a02c now writes rotation columns in Mac order before translation, preserving the original behavior when storage overlaps. `MatrixForViewer` at 0x2b87e computes the first translation from the already-negated axis, including its signed-zero behavior.

`GetRotatedBounds` at 0x2c674 now selects endpoints by the axis sign bit, including negative zero. `YawToAxis` at 0x2d5ba retains subtraction from positive zero and addition to positive zero for its second axis.

The unordered comparison paths now match Mac in `DiffTrack` (0x2c220), `BoxDistSqrdExceeds` (0x2c540), `ExpandBoundsToWidth` (0x2c452), `WindingContainsCoplanarPoint` (0x2d0ee), and `AxisToAngles` (0x2dcc2). In particular the width selection uses a rounded subtraction, and the box-distance minimum retains the same unordered/tie operand.

The packed angle conversion in `AngleMod` (0x2b99c), `AngleNormalize360` (0x2ba88), `AngleNormalize180` (0x2cd68), `AngleDelta` (0x2cdac), and `DiffTrackAngle` (0x2ce40) now handles Mac `CVTTSS2SI` invalid inputs explicitly. Its integer-indefinite result masks to zero; a direct C float-to-int conversion does not preserve that contract on arm64. `irand` at 0x2c0aa now expresses the range subtraction and result addition as 32-bit wrapping operations, matching the Mac integer instructions without signed C overflow.

## Verification and limits

All changes are under `COD2_X64`. Selecting the legacy branches produces exactly the starting source text, byte for byte. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. Existing sway, FX primitive and water fixtures were inspected; no tests were added or changed. The corrections are compiled and reviewed, not exercised. Full CI and replays remain with the integration owner.

The 122 unchecked entries comprise 106 Linux-only helper rows without an established complete Mac inline identity, and 16 unrelated caller functions attributed to this header. The inline pass established complete copy, comparison, dot, length, scale, clamp, transform and sign-predicate operations in exact caller ranges. The Dvar_ValuesEqual dispatch table at 0x328b18 was decoded to identify the two-, three- and four-component comparisons and byte comparison. Similar arithmetic alone was not treated as proof of a whole caller. Remaining helper evidence names searched reference callers, or records an absent native body/use contract. For example, AxisClear and AxisIdentity have only unreferenced reference definitions; FloatAsInt has a known sign-only use but no complete raw-integer-return identity. Q_rint and I_fround have a located floor/conversion block, but exceptional C conversion behavior in the unrelated native caller remains unverified. The private `universal-math.routing.json` maps all misattributed callers to their actual source and module for later ownership correction. No existing reconciled verdict or platform decision was changed.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-math.noindex/`.
