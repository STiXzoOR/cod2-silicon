# R3: conserve unknown bytes in a structural import view

A private derivative ledger now classifies **2,534 previously unknown bytes** as
loader-managed bytes in the original five-byte import slots. It preserves the
accepted generation 15 ledger and coverage unchanged. The structural accounting
is **10,384 = 2,534 classified + 7,850 still unknown**; no bytes disappear from
the denominator and no new instructions or function bodies are asserted.

All 633 slots span 3,165 bytes, including 631 bytes already covered by accepted
instructions. Raw Mach-O section, indirect-symbol, nlist and library-ordinal
records bind every slot. The historical dyld source explains the slot mechanism;
it does not identify the loader that actually ran this binary.

All 4,621 region histories retain their exact preceding raw transition and hash.
The two supplemental unknown bytes remain present without invented history IDs.
All 8,115 function identities, 7,481 C artifacts and accepted metadata remain
bound to unchanged inputs. The ledger keeps **633 symbol obligations and all
11 library obligations unresolved**, including binding, implementation behavior,
nonreturn behavior and native replacement equivalence.

The independent parent replay reproduced all 12 manifest entries exactly and
verified 23,118 input hashes unchanged. A separately authored parent audit parsed
the original Mach-O again and checked the complete byte partition, raw slot
bindings, every history, supplemental bytes, function identities and runtime
obligations. Thirteen effective malformed controls were rejected for their exact
intended reasons; the unchanged control remained valid. No JVM, native build or
runtime execution was needed for this static derivative.

Private evidence under `reconstruction/phase-r3/`:

- `20261023-import-structural-classification`, worker seal SHA-256
  `1a928e495a2101b840d097c3cc90861aa1cba4409a1e2fd215196f2bc596e205`.
- `20261023-import-structural-parent-audit`, fresh replay seal SHA-256
  `b4bae4d56068943d8fa12aad06f1a93c628b6b2d0d14f281732fe469c5797413`.
- `20261023-sixteenth-wave-parent-verification`, independent raw audit and
  integration evidence, including the preserved manifest self-entry check failure.

The 7,850 count describes the derivative's remaining structural unknowns. It is
not a new accepted generation, a runtime completion score or proof of imported
implementation equivalence. No installed tool or game behavior changed.
