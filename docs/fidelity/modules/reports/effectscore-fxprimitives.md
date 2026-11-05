# EffectsCore/FxPrimitives module reconciliation

Updated 2026-11-05.

**201 rows examined: 162 equal, 17 fixed, 21 intentional, 1 unchecked.**
All 127 Mac entries were compared. Linux supplied names and inline identities;
its implementations were not the behavior authority. The follow-up below resolves 14 allocator identities using Mac ownership, layout
and source-line metadata. `MoveToFront(FxMemBlock *)` remains unchecked.

## Corrections

**Lines retain their fixed endpoints.** The complete Mac `Line::Update` body at
0xa62d6–0xa6638 contains no particle-motion call. Native advanced line origins,
wrote an extra second radius and skipped color at zero size. It now copies or
transforms the fixed origin and endpoint, samples radius 0, then updates color.
The valid-line attachment expectation introduced by `681dc52` was corrected in
the existing fixture; attachment invalidation and caching remain intact.

**Tail update order and animation match Mac.** Mac samples size before length
(0xa5eb4), updates RGB/alpha and material subimage (0xa5ffd–0xa601f), then
calculates its endpoint (0xa6076). Native calculated the endpoint first, omitted
animation, wrote radius 1, and set the stop flag at zero size. These differences
are removed. Tail's previous-origin history and single bolt transform remain.

**Clouds preserve their previous local origin.** Mac saves it at
0xa66b2–0xa66d7 and writes it to renderer endpos at 0xa6adf–0xa6b01. Native
omitted this write and queried material animation even though the cloud update
has no subimage call. Native now retains the sampled height scalar at the field
corresponding to Mac 0x258, and follows the original size/height, color, rotation,
quaternion, dimension-remapping and final-position order.

**Update boundaries are preserved.** Tail, line, cloud, oriented-particle and
particle negative normalized times now clamp to zero instead of returning false
(Mac 0xa60d8, 0xa65d6, 0xa6def, 0xa76f8 and 0xa7c6f). Ordinary particles return
true on either zero radius before later appearance work (0xa7b61–0xa7b66,
0xa7d60–0xa7d69); oriented particles continue, as their separate body does.
Oriented-particle appearance/animation now precedes its final world transforms
(0xa7200–0xa740a).

**Cylinder culling passes the original radius order.** Mac 0xa0a23–0xa0a33
passes radius 1 before radius 0. Native previously reversed them.

**Numeric branches retain SSE behavior.** Subimage updates preserve negative
frames until the signed frame-count boundary, retain negative loop counts and
use defined cvttss2si/int32 wrapping behavior (0xa80dd–0xa819c). Color conversion
retains the distinct RGB and Flash wrapping subtract/negate sequences at
0xa1e62–0xa1e89 and 0xa0e98–0xa0eb1. RGB always applies its lower clamp to the
original converted value after selecting the upper-clamped candidate; Flash
can return 255 early. The first reconciliation incorrectly shared Flash's
early exit with RGB. Independent review identified the difference: RGB
`-8421504.0f` converts to -2147483520 after scaling and must produce zero,
even though its upper-clamp subtraction wraps positive. The follow-up keeps
that final RGB lower clamp and leaves Flash's sequence intact.
Gravity sampling defines the Mac overflow/NaN
conversion at 0xa3ec1. Alpha selects zero for unordered input like
0xa5293–0xa52cc. Cloud extent selection, particle visibility and tail endpoint
length retain their original unordered comparison branches at 0xa08b5,
0xa0be4 and 0xa127f. Finite curve interpolation operation order is unchanged.

## Preserved native contracts

Native objects, curve pointers, vtables and linked frames retain their widened
layouts. `FxBoltFrame::Acquire` still uses zeroed `sizeof(FxBoltFrame)` storage
and matching native array new/delete wrappers; Mac allocates/zeros 0x44 bytes
at 0xa109c–0xa10c1. Renderer submission uses aligned `sizeof(GfxEntity)` storage
and full-width material/model pointers while retaining all field and flag
mappings in Mac 0xa0524–0xa062c. Death/impact children keep the existing native
normal-to-full-axis scheduler callback adapter.

Defined native null handling is deliberately preserved. Mac bolt archive
replacement increments address zero at 0xa1b22 after a negative handle with an
existing frame; native releases and clears the slot. Mac `Particle::SetAxis`
zeros its first vector on null input, then reaches a low-address read through
0xa0de9–0xa0dee; native zeros all three vectors. Native `Effect_SetBoltFrame`
also accepts a null wrapper to clear the slot. No null writes, low-address reads
or indeterminate native storage were introduced. The existing subimage guard
for renderer frame counts <=1 remains defined for invalid renderer counts.

## Verification and limits

No new tests or cases were added. The existing primitive fixture passed with
stock and CoD2x compile flags under ASan/UBSan. It exercises creation, motion,
attachments, scheduler dispatch and renderer submission. It does not exercise
archive streams, all cull callback arguments, negative-frame animation or the
new nonfinite/overflow branches; those are compiled and reviewed only. No Mac
binary execution, renderer capture, full CI or replay was run.

The arm64 Release `cod2_macos_ded` target builds with two low-priority jobs.
That target excludes client FX. FxPrimitives.c also compiles separately with
the exact configured stock `cod2_macos` production command, including NDEBUG,
with its output object redirected into private evidence. No client was linked.
After the RGB review correction, that production compile and both existing
stock/CoD2x sanitizer fixture runs passed again; no tests or cases were added.
`unifdef -UCOD2_X64` output for FxPrimitives.c is byte-identical to the base,
and the file contains no `__LINE__` expansions. This is disabled-source
preservation, not an original i386 object comparison.

The fixture also supplies a disabled `com_statmon` dvar and a no-op warning
adapter for the concurrent FxUtil allocation-limit warning correction. This is
a cross-unit linkage dependency; the parent owns combined integration testing.

Private evidence is in
`~/Library/Application Support/CoD2-native-decomp/recon-fx-primitives.noindex/`.
Exact-symbol disassembly was used, with `__textcoal_nt` explicitly selected for
the Effect visibility and Flash lifecycle symbols. Triage and constant checks
were supporting checks, not semantic equality proofs. The private unit ledger
records every assigned ID and each remaining identity gap.

## Integrated batch verification

Integrated source 5bf663a passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

## Allocation identity follow-up

The follow-up on base `053afddc` closes 14 previously unchecked rows without
changing production source: the manager constructor, Alloc, Free, FxBoltFrame
new/delete, and nine primitive class delete operators. Existing resolved
status/evidence is retained exactly.

This rests on positive Mac metadata as well as executable consumers. STABS
defines each relevant `FxMemMgr<T>` as a one-byte class with no data fields and
names its constructor, Alloc and Free. Owning class records name their new/delete
operators. The primitive global initializer at 0xa04b4–0xa04b8 has no work. Raw
SLINE/SOL records attribute the allocation at 0xa109c–0xa10c1 to FxMemMgr.h lines
24/25/27 and the free at 0x9fe9d/0x9fea5 to line 35 of that header. Thus the
manager in this Mac build is a stateless array allocator, not an unidentified
32KB block pool.

The hand-read allocation portions of FX_AddCloud, Flash, Light, Cylinder, Line,
Particle, Tail, Emitter and OrientedParticle allocate their exact Mac object
sizes, conditionally zero the allocation, then enter their constructors. Native
FxUtil uses the corresponding `sizeof` sizes and full-width class layouts.
FxBoltFrame acquisition allocates and clears 0x44 Mac bytes; native uses
`sizeof(FxBoltFrame)`. Its list owns a separate allocation, retains it through
reference increments, unlinks it at zero references and array-deletes its base.
No object pointer is masked into a block.

Each owning class delete boundary was checked separately:

| Owner | Mac deleting destructor | Object array-delete site |
| --- | --- | --- |
| FxBoltFrame | Release 0x9fe6c | 0x9fea5 |
| Effect | 0xa1b82 | 0xa1bdf |
| Light | 0xa28bc | 0xa2919 |
| Particle | 0xa27c0 | 0xa281d |
| Cloud | 0xa9a6a | 0xa9ac7 |
| Line | 0xa9b56 | 0xa9bb3 |
| OrientedParticle | 0xa9876 | 0xa98d3 |
| Tail | 0xa9c42 | 0xa9c9f |
| Cylinder | 0xa9712 | 0xa976f |
| Emitter | 0xa9d2e | 0xa9d8b |

Flash's coalesced destructor at 0x2ef972–0x2ef9e1 was also read directly from
`__textcoal_nt`; its final delete at 0x2ef9cf reaches the same array-delete
stub. This does not add a Flash ledger row. Native deleting-destructor vtable
slots, bolt release and `macos_cpp_abi.cpp` array-new/array-delete adapters
preserve these ownership boundaries with native pointer widths. Only these
allocation boundaries are certified by this follow-up, not complete FX_Add
callers or the unverified Linux block helpers. `MoveToFront` stays unchecked:
there is no established Mac block-layout/consumer identity for that helper.

Exact stock and CoD2x arm64 Release commands compiled all four assigned source
files, FxUtil and macos_cpp_abi.cpp. Both existing primitive ASan/UBSan fixtures
passed, including nine primitive lifecycles and bolting. Their allocation
adapters use calloc/free, so the production C++ allocator was compiled and
reviewed, not runtime exercised. Archive allocation, exceptional allocation
failure and the unresolved helpers were not exercised. No tests, cases,
assertions, full CI or replays were added or run. Triage and constant checks
were advisory; the Release helper name did not resolve in the constant tool,
so its exact-symbol disassembly was used. Production source is unchanged,
including OFF text and actual `__LINE__` expansions. Private evidence is in
`~/Library/Application Support/CoD2-native-decomp/recon-fx-alloc-remaining.noindex/`.

## Original integer-selector provenance follow-up

The original Linux ELF preserves calls from seven cull wrappers to the common
plane-count operation at 0x81247a4, which calls the original I_min copy at
0x81293be. That supplies the helper identity; the Mac operations decide behavior.
Particle, OrientedParticle, Cloud, Line, Tail, Cylinder and Light culling at
0xa0758, 0xa07e4, 0xa0870, 0xa090a, 0xa097e, 0xa09f2 and 0xa0a6c each test the
sign after a wrapping 32-bit camera-count minus five when flag 0x02000000 is set.
An ordinary signed comparison selected the wrong result for INT_MIN through
INT_MIN+4. A shared native selector now performs the unsigned subtraction and
sign-bit selection. Existing position, radius order and Cloud extent behavior
remain intact.

The backing contract is defined: FxCamera owns six plane records and a signed
count. Mac CalcFrustumPlanes writes five at 0xab55c, optionally initializes the
sixth plane and writes six at 0xab5aa, and initializes plane distances before
returning. Native preserves that layout and producer contract. The corrected
exceptional count selects the first five backed records; it introduces no
uninitialized storage. Normal producer counts five/six already agreed, and this
review does not establish normal gameplay reachability of the extreme counts.

Five previous equal owners become fixed; Cloud and Cylinder were already fixed
and gain this additional correction. The module is now 157 equal, 22 fixed,
21 intentional and one unchecked. Manual compilation of the changed translation
unit passed in stock and CODX client configurations; this FX file is excluded
from dedicated targets. COD2_X64=OFF source is byte-identical after conditional
selection. No tests, CI, replay, full build or runtime exercise was performed.
Private evidence: recon-provenance-math4.noindex; compilation receipts are in
manual-fx-cull-compile-4bac2b6de38b.noindex.

## Original block-promotion member identity

Original1.0 FxMemMgr<FxBoltFrame> MoveToFront0x8127d96 and original1.3 specialization0x812a1b8 establish the generic member recorded by the outstanding row. Actual original manager allocation/free calls at0x812a115/0x812a238 reach this unlink-and-promote operation. The complete original bolt ownership chain is now tied to the complete Mac Acquire/Release owners and the positively identified stateless Mac FxMemMgr backend. The earlier allocation follow-up lacked this original consumer bridge.

MoveToFront is intentional as internal pool bookkeeping replaced by the existing direct array allocator, with the same explicit pool-capacity/reuse/accounting/failure exclusions documented in the FxMemMgr report. No standalone block-promote operation is invented, and no prior owner verdict changes. This leaves157 equal,22 fixed,22 intentional and zero unchecked entries in this module. Independent original/Mac/native review passed; no source change, compilation, tests, CI, replay or runtime exercise occurred.
