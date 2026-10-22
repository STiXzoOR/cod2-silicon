# Original server prototypes and argument storage (R3)

Scoped original STABS recovers `void SV_ConnectionlessPacket(netadr_t, msg_t *)`
and `void SVC_Info(netadr_t)`. The original by-value network address is 12 bytes:
a 32-bit enum at offset 0, four IP bytes at 4, a two-byte port at 8 and two tail
padding bytes. Both functions receive it at EBP+8; the message pointer follows
at EBP+20. This is static declaration/storage evidence, not full behavior closure.

The independently parsed type graph has 13 nodes. Its self-referential type
number encodes void, and both function records refer to that type. Linkage names
corroborate argument types but do not encode these return types. Raw parameter
records and original frame instructions corroborate stack storage. The current
native 20-byte address includes IPX storage; this intentional seam is not an
LP64 widening of the original type and is unchanged.

The historical `int` to `undefined4` drift affects the first scalar parameter
chunk, corresponding to the address enum. Both displayed return types remain
void. Current pseudocode still displays the SVC_Info call without an argument,
although original instructions transfer its argument before the tail jump.
Stored DEFAULT signatures remain incomplete. The display omission is a source
fidelity gap; its decompiler cause and response to explicit signatures remain
unverified. No annotation or generation promotion occurs here.

The worker bound 1,077 raw spans, all 860 target instructions, a 135-instruction
direct-caller prefix and two tail displacements. Its separately authored audit
verified eight controls with exact mutation paths and intended rejection reasons.
Parent replay reproduced six semantic artifacts byte for byte; an additional
parent parser checked raw nlist records, scoped types, stack offsets, branch
bytes and the complete displayed declarations.

Natural i386 alignment is ABI-derived; an explicit alignment attribute and C++
method semantics are not recovered. Padding values are not inferred from field
layout. A wrongly labelled exploratory address window was excluded from final
evidence. Failed worker and parent audit attempts remain retained; parent errors
were a wrong expected scoped pointee and an assumption that declarations occupy
one line. Startup capture limitations are documented privately.

Private phase-r3 evidence:

- `20261023-server-prototype-recovery`, seal
  `494a605f020395e1f7e162d944966b0d825db183d0f237ec943aa2b876f2a1ef`.
- `20261023-server-prototypes-parent-fresh`, seal
  `3334040abb1189d1ebb68eb9590874b94adac216c28cd73f2d04d5bcdabf141c`.
- `20261023-fifteenth-wave-parent-verification`, parent raw checks and receipts.

No native code/tests changed; private decoder, raw audit and controls provide the
relevant verification. A future disposable i386 signature preflight must measure
metadata and pseudocode effects while preserving original bytes and separate
function identities. The native address layout must not be replaced by this
original-i386 evidence type.
