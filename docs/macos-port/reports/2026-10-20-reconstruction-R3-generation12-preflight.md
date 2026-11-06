# R3 generation-12 application preflight

Parent verification accepts the combined initializer and inflate contract
against accepted generation 11. The contract remains unapplied. It creates
156 instruction records covering 516 bytes and replaces 11 complete old records
covering 25 bytes. The prospective recovery is 491 unknown bytes, leaving 11,335;
the actual accepted baseline remains unchanged at 11,826.

Three exact body additions retain all 8,115 function identities and 4,621
original-region identities and their histories. There is no no-op body write
for inflate. All 618,905 unaffected full instruction records remain bound by
digest, including their pcode, flow, overrides and references.

The contract requires 43 direct-control references, all currently missing,
and one conditional local Resume flow adjustment. The computed allocator call
remains targetless. Exactly one complete malformed READ reference may be
removed; all other existing references, including all 108,649 DATA records,
must survive. The incoming DATA reference into an original MOV remains at its
original byte address. The minimum prospective reference count is 389,759;
automatic additional effects require separate review.

The fresh parent replay scans all 618,916 current instructions and 389,717
references, checks all 156 proposed instructions with Capstone and LLVM,
rejects 20 semantic contract mutations, and reproduces 41 characterization
files byte for byte. The parent separately checks the exact source-proposal
union, raw branch displacement operands, byte partition and ownership against
all existing bodies. All 178 manifested outputs, the complete 180-file sealed
set and 50 consumed input identities pass verification.

Fresh characterization reuses its producer algorithm. It is not a second
independent execution proof for the initializer or inflate routines. Their
accepted finite execution evidence, original ordered tables, LSDA records,
library and frame assumptions remain mandatory. Four unrooted inflate NOPs
remain unresolved. Other pending cleanup proposals contribute no operations
to this contract.

Private worker seal SHA256:
`9d95c28ff74285f8ce07cb603cde8dd2ddc02d50ce61a7f683f3622892a9f0fc`.
Parent acceptance SHA256:
`635540551eaafabcf792e6d578a29eb7919c7b213fbc5f954eb0076d10c2622d`.

No project was opened or hashed, and no native behavior changed. Native suites
were not rerun for this report; original-byte and complete metadata checks are
the relevant verification. Application still requires complete export, metadata,
history and pseudocode-impact audits. No global callee flags, exception edges,
data listings, symbols or denominators may be changed by this contract.
R3–R6 and full port certification remain open.
