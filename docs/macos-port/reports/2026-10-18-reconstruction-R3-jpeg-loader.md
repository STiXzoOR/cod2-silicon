# R3 — JPEG, exception and loader discovery

Status: this bounded MP discovery checkpoint is independently verified.
Full decompilation and validated reconstruction remain incomplete. Metal remains
paused. This follows the [compression and cleanup checkpoint](2026-10-18-reconstruction-R3-cleanup.md).

## Discovery changes

Generation 08 adds 1,330 exact instruction records covering 5,012 original bytes
and identifies 48 bytes of guarded dispatch tables. Sixteen existing function
bodies gain the supported code. All 8,115 inventory entries remain. No original
instruction is replaced, and no original binary byte is patched.

| MP discovery measure | Generation 07 | Generation 08 |
| --- | ---: | ---: |
| Decoded instructions | 613,770 | 615,100 |
| Unknown executable bytes | 29,934 | 24,874 |
| Unknown instruction-section bytes | 26,825 | 21,765 |
| Physical unknown intervals | 4,630 | 4,618 |
| Owned computed sites without static targets | 2,686 | 2,694 |

The last count increases because discovery exposes 13 indirect sites while
resolving static targets at five existing sites. The additions comprise 11 calls
and two jumps. The five resolved sites are two guarded JPEG switches and three
initial lazy-loader jumps. Neither finite callback observations nor initial
loader targets establish exhaustive runtime dispatch.

## Evidence and its limits

JPEG recovery uses 114 synthetic original-instruction cases: 58 return and 56
stop before unmodeled imports. Decoder agreement, ordered table reads and
original unsigned guards support the exact additions. These cases do not prove
whole-game caller reachability or native JPEG equivalence.

Exception recovery uses original debug and unwind metadata, agreeing decoders,
control-flow checks and exception-argument tracking. Its ordinary-return
calling-convention assumptions remain explicit. Thirteen new call instructions
receive local no-return analysis metadata; existing instructions and global
import flags retain their settings. Original exception propagation was not run.

Nine loader cases execute 81 original instructions along the initial lazy path,
stopping before the unresolved external loader transfer. Three thunk bodies
expand from 13 to 25 bytes while retaining imported identities and signatures.
No loader service result is modeled.

The analysis adds 455 required control-reference tuples and 39 automatic data
reads from exact new instructions. All existing references survive. Nineteen
default label displays and five derived jump-target arrays change as a result
of the accepted additions. Exact before/after records and a complete original-
instruction digest account for those effects; no blanket metadata exception is
used. Existing bytes, flow overrides, fallthrough and intermediate operations
remain unchanged. Failed application and verifier attempts are preserved.

## Export and verification

The fresh export contains 7,481 decompiled bodies and 634 external entries. All
8,115 status records and every internal evidence/pseudocode hash pass independent
parent checks. All 94 fresh diagnostic probes match the full export. Eight switch
mappings match 69 ordered slots and eight defaults, including the six prior
mappings. Only two JPEG functions change their decompiled C; the other 7,479
internal C files remain identical. Warning counts stay at 1,261 functions and
1,621 warning lines.

Within the same prior 72-function diagnostic scope, intermediate-code sites in
unknown bytes fall from 458 to 285. The 22 added probes introduce no additional
outside-body sites. Three competing instruction boundaries remain unresolved;
none is promoted to accepted code or ownership.

All 8,115 function transition records and 4,621 region transition records retain
their prior identities. No denominator removal or padding classification lowers
the unknown counts. The private freeze covers 31,319 evidence items and 125,069
input files; read-only export preserves all nine project files.
A separate parent read-only project replay reproduces all 7,496 saved discovery
and metadata files and preserves the project and frozen inputs.

All binaries, decompiled C, traces and detailed evidence remain private and
outside Git. This checkpoint changes no native implementation or Metal
behavior. Original-byte, state, ledger and export verification supplies the
evidence for this documentation-only change; native runtime tests would not
validate it. Discovery, source correspondence and the remaining requirements in
the [reconstruction plan](../../plans/2026-10-15-validated-reconstruction.md)
remain open.
