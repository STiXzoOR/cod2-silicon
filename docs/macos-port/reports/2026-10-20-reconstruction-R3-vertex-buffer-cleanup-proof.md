# R3 vertex-buffer cleanup proof

A qualified, unapplied original-byte proof covers the two CVertexBuffer destructor
variants. It proposes 32 instruction creations covering 92 bytes and two additive
body updates of 46 bytes each. Both final unknown NOPs remain unknown. No existing
instruction is replaced, no typed data is removed and no accepted project changes.

Original debug extents and ordered exception metadata identify each cleanup root
and its protected calls. Independent LLVM and Capstone decoding checks the full
337-byte extent of each destructor. The proposed cleanup records preserve every
existing full instruction, reference, function record and history outside the two
specified body unions. All 8,115 function and 4,621 region identities remain.

The original cleanup loads the next list link before deleting the current node,
then calls the base destructor with its saved argument and forwards the incoming
exception token to Resume. Original instructions and a separately implemented
interpreter agree for 64 cleanup cases across list lengths zero, one, three and
five, two noncontiguous layouts, two tokens and two deletion poison patterns.
All cleanup instructions and edges are covered.

A further 44 cases execute original entry prefixes through selected protected
calls before supplying the explicit unwind transition to cleanup. These witness
the frame, object, sentinel and saved base argument. The virtual-call target is
a fixture; it does not establish the process's actual binding. The combined 108
positive cases compare ordered traces, calls, original stack writes, registers
and complete finite memory state. Twenty-four execution/domain negatives and
five proposal corruptions are rejected.

Parent fresh external replay reproduced the saved proof and all stated
positive and negative checks. A separate original-byte decoder and full
metadata scan confirmed the exact proposal preconditions against both
generation 11 and accepted generation 12, including body ownership, existing
instruction conflicts and absence of typed-data overlaps. The complete sealed
artifact set was rechecked after replay.
Parent acceptance receipt SHA-256: `67356aa7e721b683008eec6dfc89260fe1a3f6c4294fe2139c2c851b59832f92`.

Operator delete is modeled as poisoning only the current owned allocation and
returning with the declared ABI preserved. The base destructor is an opaque
ordinary-return fixture with no visible memory effects. Live unwind delivery,
real callee behavior, concurrent access, cleanup exceptions and arbitrary aliases
are outside the proof. No whole-destructor or native equivalence is claimed.
A later preflight must capture actual decoder/default-reference effects and rebase
the full global preservation contract before any application.

All original bytes, detailed metadata, traces and replay artifacts remain private.
Native runtime code and the accepted generation are unchanged. Remaining R3
closure and R4–R6 certification are separate unfinished work.
