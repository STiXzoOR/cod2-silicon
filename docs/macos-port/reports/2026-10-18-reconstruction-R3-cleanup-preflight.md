# R3 cleanup preflight checkpoint

Two private proposals are accepted for later application. Neither has changed
the analysis project or the accepted decompilation export. Reconstruction and
native runtime equivalence remain incomplete; Metal work remains paused.

## Four-unit cleanup contract

Fresh parent verification checked all 618,345 existing instruction records,
replayed four source verifiers, and independently decoded all 343 proposed
instruction records with Capstone and LLVM. Fourteen negative controls passed.
The final freeze check verified 12,692 source files and 426 output files.

The exact proposal replaces 17 old orphan instruction records with explicitly
bounded new decoding, for a net increase of 326 records. Its 1,130 decoded
bytes include 42 replaced bytes and 1,088 previously unknown bytes. Thirty
function-body unions add 1,353 bytes, including 223 already decoded unowned
bytes. All other instruction metadata remains unchanged.

The contract requires 153 control-reference postconditions: 22 already exist
and 131 must be added. Three explicitly identified spurious control references
are removed; all 108,649 existing DATA references remain. Eleven new call-site
flow overrides are permitted. Existing local overrides and global function
flags remain unchanged.

After application, the expected counts are 618,671 instructions and 12,733
unknown executable bytes. These are prospective figures, not completed export
results. All 8,115 function identities and 4,621 original region identities
remain in the denominator.

## Fragment-preserving proposal

A separate proof supports four additive cleanup units: 33 instructions covering
90 unknown bytes, seven new direct-control references, and one conditional
new-site non-returning call override. Existing function fragments remain
separate and retain their bodies, identities, symbols, and incoming references.

Two other units receive no operations. Their raw decoding conflicts with three
existing instructions and one defined-data range. They require a separate
replacement contract before application.

The parent replay reproduced all 95 core proof files, five control-postcondition
files, and four symbol-preservation files. Nine core negative controls and five
supplement controls passed. The final freeze check covered 49 source files and
610 output files. The parent also compared all 8,115 prospective inventory
records; only the four permitted bodies change.

## Qualifications and next step

These are static contracts under explicit exception-register, landing-frame,
ordinary-return ABI, and memory assumptions. They do not establish exception
execution, arbitrary callee behavior, unified pseudocode, or whole-function
semantics. Replaying a verifier supplements source review; it is not a second
independent proof algorithm.

Apply the first contract to a fresh private project, then audit all resulting
metadata and export effects. Automatically generated references or labels are
not preaccepted. The separate fragment-preserving proposal remains unapplied.
All binaries, decompiled output, detailed proofs, and traces remain outside Git.
