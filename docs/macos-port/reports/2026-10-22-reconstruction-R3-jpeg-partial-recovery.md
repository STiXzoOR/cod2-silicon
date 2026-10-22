# JPEG switch correction remains unresolved

The helper-only correction is rejected. A clean parent rebuild and independent artifact audit reproduce four cases in which it retains an incomplete switch target list. No candidate decompiler, installed tool, accepted project or native production change is accepted by this result.

This follows the [candidate review](2026-10-22-reconstruction-R3-jpeg-candidate-review.md). The new experiment handles concrete memory writes that appear as normalized `COPY` operations, including writes whose original store marker has been lost. Seven direct helper cases pass, but that does not make the correction sound through the full recovery sequence.

Synthetic i386 execution demonstrates the failure: a write changes the guarded word, and the later dispatch reaches case 9. Both added recognition paths recover only cases 0–5. Stage traces show the write in the raw temporary function, then its absence after the `jumptable` action. Partial recovery installs six targets. The corrected helper rejects the write during later full-flow analysis, but the six targets remain, now with address labels. Reachable case 9 is still absent.

The parent rebuilt all 94 objects from the frozen 237-file source family. It reproduced 15 retained control results, six memory fixtures, 52 upstream assertions, seven helper cases, four barrier-mutant outputs, six stage traces and 200 synthetic i386 paths. All nine accepted switch contracts and the original JPEG's six ordered labels, targets and duplicates remain unchanged. The four soundness failures remain failures; successful reproduction is not candidate acceptance.

A separately authored parent audit checked the complete input grid and instruction trace against fixture bytes, reconstructed all 600 complete allocation captures from 316 writes, and checked 2,896 executed instruction entries. It derived the missing targets from actual dispatch indices and branch successors, then compared those with the recovered tables. All four negatives retain six targets while omitting reachable case 9; both positive fixtures retain their reachable targets. The audit also verifies the observed ordering of partial installation and later rejection.

The source and worker replay inventories contain 2,936 and 2,336 files. The parent rehashed both complete read-only inventories and their recorded input dependencies. Its clean replay contains 2,331 inventoried files; the supplemental audit rehashed all 1,177 captured build dependencies. The final parent adjudication binds 2,901 input records and preserves the exact compiler, SDK, source, driver and result evidence.

Failed attempts remain preserved. The initial worker replay compiled all 94 objects but failed to link because an absolute map filename contained an unquoted space. A copied driver bound the frozen source root and used a relative map filename; the parent ran that corrected complete driver from a fresh directory. An early common-phi fixture did not exercise the intended recognition path and is excluded from that claim; the refined fixture does. Three malformed full XML documents and two existing type-order adaptations remain retained, with switch fragments parsed separately. A stale preparation hint is not used as execution evidence.

Private evidence identifiers:

- Source seal: `1a2516d8fc1e2b05513e7bae18ace40d9aee2f285e6b580b741578c7bebdaeb8`.
- Worker replay seal: `f942f713fa723d328ba4783f6748458a24c497d6ad974890dd52abf86ac65c9e`.
- Parent replay seal: `d963e44126226e6cb50421e3de17eee2ed62a8ea3a668ed6510746e0eab7e37a`.
- Parent adjudication: `634565c7796a234575c24ce09e921b661db927c83cfdb3d5d263723a7984559a`.
- Parent audit seal: `1c16245813adc387c68d71feb5261e376e35408642b465123a48c24e9d52ae65`.

The executed domain is synthetic i386 input words 0–9 with selectors 0 and 1. This establishes neither full JPEG semantics nor exhaustive alias, loop, architecture, native-game or hardware equivalence. Host runtime and shared-cache qualifications remain in the private closure. Native tests were deliberately omitted because no native source or build mechanism changed.

Partial-recovery refusal and broader action/dead-code changes remain separate, unimplemented follow-ups. The accepted reconstruction generation remains 15. Full reconstruction remains incomplete.
