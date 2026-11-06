# Bounded server tail-transfer proof

Accepted after an independent parent replay and supplemental audit. This establishes a bounded original-instruction transfer from `SV_ConnectionlessPacket` into `SVC_Info`; it does not apply a reconstruction change or change native production behavior.

The sole proposal is one unapplied `CALL_RETURN` flow annotation at the existing tail jump. Separate original symbol extents and frame records agree with two instruction decoders. Both original function bodies and their identities remain separate. A disposable-project preflight must measure complete metadata and export consequences before application.

The frozen domain contains 192 cases: 96 starting at the original entry under explicit service models, and 96 starting conditionally at the local branch. Cases vary address fields, incoming and local padding, saved registers and valid stack placements. All cases preserve the caller return slot, entry stack pointer and four saved registers, and transfer all 12 original argument bytes. The two padding bytes come from the initial local stack; they need not equal incoming argument padding. The target's actual argument-copy prefix reads are retained.

The parent replay passed all 192 cases and 10 controls, including a same-value padding-provenance mutation and an ineffective-control check. It reproduced 2,395 complete semantic/case artifacts byte-for-byte and all 216 source-run immutable input identities. The artifact audit checked 15,552 original instruction records and 768 complete allocation comparisons.

A separately authored parent audit parsed original address mappings and branch displacement directly, checked the complete case grid and every traced instruction byte, and reconstructed captured memory from all 13,536 read/write events. All 768 final allocation comparisons matched. It also checked all 1,056 modeled call records for original call-target binding, frame continuity and saved-register preservation. These models do not execute service bodies.

Coverage is 102 of 512 connectionless instructions and 18 of 348 target instructions. The remaining 410 and 330 instructions are unexecuted. Entry execution is conditional on the frozen service models and initialized state. No parser, tokenizer, server side effects, target output, whole-function, native/game or original hardware/process equivalence is established. Native tests were deliberately omitted because no native behavior changed and the original 12-byte argument layout differs from the current native 20-byte layout.

All 177 affected decompiler sites are enumerated but remain in the evidence ledger. The 25 cleanup sites stay deferred. No function, reference, symbol, data, instruction or unknown-byte metadata changed. The 8,115 function histories and unknown denominator of 10,384 bytes in 4,431 intervals remain intact.

Failed preparation and harness attempts are preserved. Two early failing traces are incomplete because of serializer and write-hook failures; final source, worker replay and parent replay retain complete traces. The worker's terminal helper failed on timestamp precision before writing its final JSON, then a separate sealing command succeeded. Neither sealed worker capsule was backfilled. Three host OS backing libraries are freshly hashed observations; shared-cache images without backing files remain observational.

The parent rehashed complete source and worker replay inventories, their consumed inputs, and both final parent inventories. All sealed evidence remains read-only. Private evidence identifiers:

- Source seal: `fb9fe7ef5bec0647463c895e1dd6b3413b92818895b070bebf8c19743ae6af82`.
- Worker replay seal: `52d0ed4bcb64d06bf379503c4433f6fd363ce53789e3d2fdcf8433986670df4a`.
- Parent replay seal: `890b561a61bb4ab970a742cc7aa695c9ef0b05569fe7418f5ca270ff792e14f9`.
- Parent acceptance: `7bae3e5e533d3eccf590b676e1d23d297219fcd29590e22b87465324f9027bfe`.
- Parent audit seal: `907c57376e100d0e95a422a7f2efaeda38c529dd65019ad593a20ea5a636043c`.

This evidence supports a narrowly scoped preflight, not generation acceptance. Full reconstruction remains incomplete.
