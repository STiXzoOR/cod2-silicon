# R3 vertex-array constructor cleanup proof

Parent verification accepts a qualified, unapplied proposal for two vertex-array
constructors. It creates 100 original instruction records covering 300 currently
unknown bytes and adds 150 bytes to each existing body. No old instruction, data
listing, reference, symbol or identity is removed. Accepted generation 11 remains
unchanged at 11,826 unknown bytes; this proposal alone would reduce that to 11,526.

Independent parsing of the original named extents, FDEs and ordered LSDA rows
roots the cleanup code. LLVM and Capstone agree on 283 instructions covering
933 bytes across both complete constructors and their helper and destructor.
The first helper call has no local landing pad. Each constructor has eleven
protected dynamic calls: three member calls and eight array iterations.

Fresh parent replay reproduces the saved semantic outputs and passes 42 finite
cases plus 18 corruption checks. The cases include normal construction, injected
local exception transfers, an alternate opaque virtual-call binding and separate
helper/destructor execution. They check reverse destruction of completed array
elements, exact object effects, ABI preservation in the stated model and the
exception token delivered to the intercepted resume call.

The two indirect calls remain targetless. The original local vtable bytes and
weak-symbol metadata do not establish the binding selected by a running loader.
The alternate binding is an explicit no-op return fixture. Actual loader binding,
unwinding, cleanup throws, unrestricted aliases and native-process equivalence
remain unproved.

The parent independently checked the exact original bytes, direct displacements,
all existing instruction boundaries, body nonoverlap, all 8,115 function and
4,621 region identities and their histories. All 389,717 references, including
108,649 DATA records, remain in the preservation contract. Existing records
relevant to the two extents are retained in full.

A mandatory parent qualification corrects an overbroad private assertion:
exported write traces contain object writes, not every stack write. The emulator
hooks and bounds-checks memory writes; operand witnesses and finite assertions
check the specified frame slots and outgoing exception token. A complete
stack-write trace is not claimed.

The proposal does not synthesize references. Before application, a separate
preflight must establish exact decoder metadata, including both resume calls
against the existing no-return callee annotation. It must audit all resulting
references and pcode without assigning concrete targets to the indirect calls.
This proposal is excluded from generation 12's already accepted preflight.

Private worker seal SHA256:
`728dd60a80c77842c04422793618378366fe2e986cce5f7527156f0869644335`.
Mandatory parent qualification SHA256:
`a8fe5f21c9dc339ac0f7ecd4acac3f87d2e5483e6d8be58dca44a2baab8a8dfa`.
Parent acceptance SHA256:
`7f68873abf14f0a8befdd334c5af0ff7b89bf051ad247ae3c2cc6aedcc3038e3`.

No native code or accepted project changed. Native suites were not rerun for
this report. Full reconstruction remains incomplete.
