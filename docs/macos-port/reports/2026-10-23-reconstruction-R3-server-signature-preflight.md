# R3: server aggregate signatures recover the missing argument

A disposable three-function preflight recovered the original aggregate parameters
and the missing `SVC_Info` call argument. **The frozen acceptance contract failed**:
Ghidra also created `/undefined *32`, which was outside the exact type allowlist.
The parent reproduced that failure. Neither run promoted a generation or changed
the installed tools, accepted project or native game.

The original-backed annotations use void returns and custom i386 storage:
`netadr_t` occupies 12 bytes at entry ESP+4, and the connectionless packet's
four-byte message pointer occupies entry ESP+16. These correspond to EBP+8 and
EBP+20 after the prologue. The recovered types are 12-byte `netadr_t` and 24-byte
`msg_t`, separate from native LP64 or IPX layouts.

Both worker and parent ran nine focused probes: the two targets and
`SV_PacketEvent` at original baseline, prior tail-annotation baseline, and after
signature annotation. Every probe completed on its first attempt without a
diagnostic. Complete original-baseline C matched accepted generation 15;
complete tail-baseline C matched the previously renewed evidence.

After annotation, both target declarations use aggregate `netadr_t` parameters.
The previously argumentless call becomes `SVC_Info(from_00)`, and high Pcode
contains one 12-byte aggregate argument. `SV_PacketEvent` C also changes through
inference; this is an observed caller effect requiring wider future verification.

The parent independently compared all stored metadata and complete probe output:
exactly two stored signatures change, with every existing local preserved. All
8,115 function identities and bodies remain intact. The complete three-function
snapshots contain 1,069 instructions, each rebound to original bytes. Signature
annotation changes none of those instructions. The earlier, separately qualified
`CALL_RETURN` override remains the sole tail-baseline instruction metadata change.

Eight intended type records and the unexpected four-byte pointer appear during
annotation, before decompilation. Immediate and final metadata are identical.
Installed Ghidra source resolves a temporary `DataType.DEFAULT` pointer while
resolving a composite pointer component. That supports the inference that the
extra type comes from `msg_t.data`; it is not an allocation trace. No stored
function references the extra pointer type. This explanation does not retroactively
relax the failed contract. A proposed exact dependency allowance remains unexecuted.

Private evidence under `reconstruction/phase-r3/`:

- `20261023-server-signature-preflight`, worker seal SHA-256
  `ff5b685797a4dd91267b17b25aa6b0c65b9a7b80eda7c3a12005569153038e74`.
- `20261023-server-signature-parent-fresh`, strict replay seal SHA-256
  `43ac246d9cc88fd1de6d0d101840322cd11bd19c1e0ada04e4e4952855e507e5`.
- `20261023-server-signature-parent-observed`, separate observational audit seal
  SHA-256 `9f271af5b1231eb5d19ddbb50543c7cde99cc9a0d58e8ac40a9b47a1e9b73ba7`.
- `20261023-sixteenth-wave-parent-verification`, independent metadata/raw audit.
  Its initial count check confused the 995-instruction static scope with the
  1,069-instruction complete snapshots; a second attempt failed only while
  serializing a shadowed variable. Both failures and corrected source remain.

This establishes bounded prototype efficacy, not source closure or native
behavior. Full export, wider caller effects and unresolved computed consumers
still require verification before accepting the annotations.
