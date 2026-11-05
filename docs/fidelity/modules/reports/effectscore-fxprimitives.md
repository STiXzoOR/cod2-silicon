# EffectsCore/FxPrimitives module reconciliation

Updated 2026-11-05.

**201 rows examined: 162 equal, 17 fixed, 7 intentional, 15 unchecked.**
All 127 Mac entries were compared. Linux supplied names and inline identities;
its implementations were not the behavior authority. The 15 unchecked entries
are Linux-only FxMemMgr template and class allocation/deallocation identities.
Mac executable callers use general array new/delete, but that does not establish
those particular Linux helper identities.

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
