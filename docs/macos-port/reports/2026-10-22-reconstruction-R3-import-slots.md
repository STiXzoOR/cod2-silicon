# R3 import-slot reconciliation — 2026-10-22

Independent raw metadata and reference checks account for all 633 five-byte
import slots in the Mac MP 1.3 executable. They support a structural classification
as loader-managed on-disk placeholders. The classification is proposed only:
accepted generation 15, its historical ledger and unknown-byte totals are unchanged.

The section occupies 3,165 bytes. All original slot bytes are placeholders;
raw section metadata, indirect-symbol entries, undefined external symbols and
library ordinals map every slot to one of 11 libraries. The accepted analyzer
records contain 630 import thunks, one non-thunk import entry and two imported
symbols without accepted functions. Each of the 631 accepted function extents
contains one terminating instruction; those extents do not implement the imported
runtime behavior. All 633 imported symbols are present.

Every one of the 14,799 recorded incoming references targets a slot start.
Independent original-byte decoding verifies 14,655 relative calls and 144 relative
jumps. The analyzer labels all as calls; its jump-site `CALL_TERMINATOR` flow may
represent intentional tail-call analysis. No incorrect target or invalid analyzer
classification is asserted. No recorded interior or outgoing reference exists
within this accepted census; raw pointers, undecoded instructions and runtime-created
references remain outside the finite domain.

Historical Apple dyld 46.16 source supports the section contract: on i386, matching
five-byte self-modifying import slots receive binder calls or resolved jumps.
Forty slots cross a cache-line boundary, which the inspected loader handles by
eager resolution when possible. This establishes a source-level explanation,
not the identity of the original runtime loader or dependent libraries. Slide,
prebinding usability, actual symbol resolution and live stub bytes are unverified.
No imported implementation or native-equivalence claim follows from this result.

The parent independently parses the original Mach-O and all 633 symbol mappings,
rederives all 14,799 raw relative targets, and checks accepted function records,
coverage categories, imported symbols, external thunk targets, reference indices
and the call census. It also reconstructs the complete unknown-byte history:
4,621 historical IDs remain, with 10,382 historical and two supplemental unknown
bytes across 4,431 physical intervals. The import section contains 2,534 of these
unknown bytes and 631 previously accepted bytes; all 3,165 bytes stay accounted for.

Seven main controls and three bounded jump/coverage controls pass in the parent
check. They reject altered slot bytes, a changed valid library ordinal, missing
slots or unknown bytes, effective interior targets, inconsistent jump targets and
a same-cardinality coverage swap. An ineffective mutation is detected and an
unchanged baseline passes. The parent reruns the bounded 144-jump supplement and
verifier, with exact agreement on its five result artifacts; it does not repeat
the worker's whole slot campaign. The worker's first all-call-encoding assumption
failed and is retained. Parent parsing failures are also preserved separately.

Private evidence is under the analysis store's `reconstruction/phase-r3`:

- `20261022-import-slot-reconciliation`: worker terminal seal
  `0beb7eaf40474484c36a896012245846727b6d08f973eb2bf6fa6fbaaeed7e45`.
- `20261022-import-slot-parent-audit`: parent terminal seal
  `2f28a9d0807dd27485a61a09f164c250d9d123288824666ec1f232446590ffcb`.
- `20261022-import-loader-parent-planning`: pinned primary loader source,
  commit `991bbf9b90afb1046fd4bd17882a9a1333421fe3`, source SHA-256
  `a46bc97e33315761570baad187debd2a51f8b188ad3b9686da2b0a0e77c0f307`.

Original binary SHA-256:
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
The worker baseline was `aca861317791e8d0a3534bf9b42061908f1a7895`;
the intervening friction report changes no consumed production or discovery input.
This static unit changes no runtime source and requires no production test run.
A later classification application needs its own explicit ledger transition and
verification; this report removes no unknown bytes.
