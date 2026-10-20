# R3 cursor and string cleanup proof

A qualified, unapplied original-byte proof covers two WinCursor copy constructors
and CStringEdPackage string cleanup. It proposes 26 instruction records covering
83 bytes, replacing two conflicting records totaling three bytes. This would
recover 80 unknown executable bytes. Three additive body updates cover 134 bytes,
including previously stored instructions. No accepted project is changed.

Original debug and exception metadata identify six cleanup roots. Independent
LLVM and Capstone decoding establishes the boundaries, and raw operands establish
eight new direct references. Incoming DATA targets remain exact even where they
point inside an original instruction; debug string offsets are not treated as
runtime code pointers. No typed-data removal is proposed. All existing references,
8,115 function identities and 4,621 region histories remain required.

A separately implemented interpreter and original instructions executed in
Unicorn agree for 480 finite cases, covering all 60 cleanup instructions and 63
edges. Cases vary cleanup root, disjoint heap or frame object placement, empty
string representation, five signed reference-count values, exception token and
callee register clobbers. They compare ordered instruction traces, calls,
arguments, writes, general registers and the token passed to Resume.

The exchange service reads and decrements the modeled reference count. Destroy
records the representation as freed with no visible machine-memory writes in
this finite model. Resume is intercepted after the actual CALL. Real import
implementations, normal constructor/string paths, live unwinding and broader
alias domains remain unproved. Fifteen execution mutations and ten proposal
corruptions are rejected; these checks do not establish whole-function or native
equivalence.

Parent fresh external replay reproduced the saved proof and all stated
positive and negative checks. A separate original-byte decoder and full
metadata scan confirmed the exact proposal preconditions against both
generation 11 and accepted generation 12, including body ownership, existing
instruction conflicts and absence of typed-data overlaps. The complete sealed
artifact set was rechecked after replay.
Parent acceptance receipt SHA-256: `1b03610ae9aeb742e94da47eeb3d8719bffd6115e33817e7f7092b0a49b26e00`.

One replacement MOV has candidate full decoder metadata derived from an identical
stored instruction. A later preflight must observe the actual processor output
and compare that full record, then characterize exact reference and symbol
effects. Complete global retention obligations must be rebased to the accepted
generation before application. CAEDesc GetData remains outside this proof because
its typed-data overlaps require separate evidence.

All detailed evidence, original bytes, traces and replay artifacts remain private.
Native runtime code is unchanged, and the accepted generation remains unchanged.
This proof advances a bounded part of R3; full reconstruction and R4–R6
certification remain incomplete.
