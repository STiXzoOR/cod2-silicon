# Original/native acceleration characterization (R2)

The bounded PM_Accelerate comparison matches all 389 baseline cases in stock
and CODX. A separately frozen 16-case signed-zero extension retains four exact
bit mismatches per variant; stock and CODX agree in all 405 cases. No production
fix is included in this report.

The original MAXSS selects its second operand on an equal-zero tie. With
stopspeed +0 and wishspeed -0, it retains -0; native FMAXNM produces +0.
The four observations start with velocity (-1,-0,-0), wish direction (1,+0,+0)
and positive addspeed, crossing acceleration and frame time +0/1. Original y/z
velocity stays -0; native y/z becomes +0. These are bit differences, although
the numerical values compare equal. A live caller producing that exact state
has not been demonstrated; stopspeed zero is permitted but is not the default.

The campaign executes the original routine and both real normalization helpers,
including their x87 return values. It compiles whole unchanged production
movement and math translation units with the actual stock/CODX Release flags.
All 165 non-debug root instructions and all 30/24 helper instructions were
covered. Debug printing, exceptional floats, other FP modes, aliasing and
whole-game reachability remain outside this finite domain.

Full guarded object images are checked within each ABI; typed state is compared
across layouts. Original reads may include initialized guards, while writes are
limited to velocity and a bounded stack window. Native arbitrary reads are not
instrumented. ABI preservation, FP controls and stack balance do not establish
hardware floating-point exception parity.

Eight controls and a hardened exact-mutation/exact-reason audit passed, including
unchanged-candidate and unrelated-error controls. The first campaign's missing-rg
failure and the parent's initial audit envelope error remain preserved. Parent
verification freshly reproduced 389+16 cases, independently checked raw function,
symbol and constant bindings, reconstructed full initialized images, and confirmed
the original MAXSS operand/result trace and native FMAXNM instruction.

Private evidence is under reconstruction/phase-r2:

- `20261023-accelerate-original-native-characterization`: manifest
  `d2ea4931c81e49498ec28cfd17b442938a4a6e76d6245e4ac0557579b131a289`.
- `20261023-accelerate-parent-fresh`: manifest
  `35d8bfffb24ef6da33510c125b1b4f3c34ae6da339c3475b12f43da4a94c0c40`.

The shared phase-r3 `20261023-fifteenth-wave-parent-verification` packet retains
independent audit sources, results, failures and replay receipts. No canonical
test changed, and broad native tests were not repeated for this report-only
unit. The next correction must add a regression, preserve the legacy branch and
verify original/native cases plus relevant native build/test matrices. The
private wider min/max proposal is unapplied and is not accepted by this report.
