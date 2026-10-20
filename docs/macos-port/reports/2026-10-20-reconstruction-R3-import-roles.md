# R3 original import-slot roles

The parent accepted a separate role and provenance ledger for all 633 original
Mac multiplayer import slots. Every slot has the same five-byte on-disk
placeholder pattern and an exact ordered mapping through the original indirect
symbol table to a requested library. No analysis project or native code changed.

Historical [Apple linker source](https://github.com/apple-oss-distributions/ld64/blob/9c2b3ef9e390d2ee41d1754251ad81ea3284c756/src/MachOWriterExecutable.hpp)
corroborates the placeholder format.
[Apple loader source](https://github.com/apple-oss-distributions/dyld/blob/991bbf9b90afb1046fd4bd17882a9a1333421fe3/src/ImageLoaderMachO.cpp)
describes how matching i386 slots become lazy-binding calls or resolved jumps.
Both source files were pinned by commit, Git blob and SHA256. This establishes
source corroboration, not the exact historical producer or loader version,
actual loader execution, resolved runtime addresses or native import equivalence.
The loader's early-binding error fallback remains part of the qualification.

The original section flags include an additional instruction attribute omitted
from an earlier diagnostic's prose; the exact raw flags are retained. All 14,768
stored incoming call references resolve to slot starts when their original
relative operands are recomputed. Their original bytes distinguish 14,624 calls
from 144 tail jumps with existing call-and-return analysis overrides.

The ledger preserves all three existing function-metadata discrepancies:
two slots lack function records and one has a non-thunk function. It also retains
all 631 stored HLT records and 2,534 unknown slot bytes. The accepted generation
still has 11,826 unknown bytes, 618,916 instructions, 389,717 references,
108,649 DATA references and 8,115 function inventory entries. Role evidence does
not subtract bytes or entries from those denominators.

The parent independently replayed the original parser and complete metadata
census, checked all 633 rows against its earlier separate diagnostic, verified
50 frozen evidence files and confirmed seven consumed metadata artifacts against
generation 11's accepted manifest. All 24 semantic corruption tests rejected
without relying on input-hash rejection.

Private evidence remains outside Git. Worker seal SHA256:
`6d446c7f0e12b5d4844be6c9264cff195b37d2c16ee5966023ac19dd4e760b4e`.
Parent acceptance SHA256:
`6f852f9530db215d5762d1c770bdf83a2a55b07583612f35c42324c04c294dea`.
The parent recorded a stray-space correction to the frozen worker report's
source link; its machine-readable source identities and verified bytes were correct.

No native suite was rerun for this evidence-only report. Original-byte replay,
metadata conservation and negative controls validate its stated scope.
R3–R6 and full port certification remain open.
