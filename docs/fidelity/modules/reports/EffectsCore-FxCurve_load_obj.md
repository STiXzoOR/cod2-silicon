# EffectsCore/FxCurve_load_obj module reconciliation

Updated 2026-11-06.

**2/2 reconciled: 2 equal; 0 unchecked.** No source change.

The complete allocator at Mac 0xac320 was compared by hand, using a disassembly restarted at the exact symbol to avoid a misdecoded prologue. Allocation, endpoint padding and key copying match. Two unusual details are retained: a padded final key copies values from the first input key, and stored `keyCount` excludes added endpoint keys. The allocator uses the matching pointer-free 8-byte curve header, so native pointer widening requires no adjustment here.

The initial review left `FxCurve_Reference` unchecked: the absolute-zero `FxCurve_AddRef.eh` marker did not identify the Linux pointer-return helper. The complete consumed pointer operation is established in the reference36 follow-up below.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.

## Integrated batch verification

Integrated source 6b334b3 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

## Allocation identity follow-up

At this earlier follow-up, the Reference row remained unchecked. Exact allocation calls
Hunk_AllocAlignInternal at 0xac38e; Mac type records place inline keys after the
two integer fields, with no reference-count member. This establishes storage
ownership, but not a return-value/alias identity between Linux Reference and
the zero-address Mac AddRef marker. The existing allocator verdict is
preserved exactly.

No production source changed. The assigned sources compiled with exact stock
and CoD2x arm64 Release commands. Both existing primitive ASan/UBSan fixtures
passed, using their calloc/free allocation adapters; they do not exercise these
unresolved helper identities or the production C++ allocator. No new tests,
cases, assertions, full CI or replays were run. See the FxPrimitives follow-up
for the shared ownership evidence and verification limits.


## Original helper provenance — fx9

The complete original pointer-return body is now anchored at 1.0 0x8121c40 / 1.3 0x81240d0. Its exact actual consumer is FxChannel_Copy: call 0x81236de passes source->curve and stores the returned identical pointer into target->curve. No original counter mutation occurs. This resolved the original pointer contract, but at fx9 the corresponding complete Mac copy/reference owner remained missing. Hunk ownership, absence of a reference-count field, and a zero-address AddRef exception marker cannot supply that operation. The row therefore remained unchecked at that stage.

The retained direct-call scan finds the positive channel-copy-to-curve-reference chain; raw absolute address references add only unwind records. Those bounded searches are not evidence of semantic absence and close no row. No source or prior resolved verdict changed, no new defect was established, and no compilation, tests, CI, replay or runtime exercise ran. Original bodies, disambiguating neighbors, decoded table and fresh exact Mac candidates are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-fx9.noindex/`.

## Complete consumed curve reference — reference36

`FxCurve_Reference` is now equal for the complete unchanged curve-pointer value operation consumed by actual iterator construction. Fresh original 1.3 0x81240d0–RET0x81240d7 returns its incoming pointer unchanged, with no pointee access, ownership mutation or other effect. Its original `FxChannel_Copy` caller consumes the returned full pointer at0x81236e3 before separately copying both range endpoints.

Actual Mac `GetDecalSize` passes its template channel4 and disjoint local iterator to `FxChannelInstance_Create` at0x61f24. The constructor loads the entire curve pointer at0x9ff96 and forwards it unchanged; `FxCurveIterator_Create` reads it at0xb9fce and stores that same pointer into `master` at0xb9fd1. Native typed pointer assignment preserves this full value. The real consumer then reloads the stored master at0x61f37 and dereferences its curve header/keys; the alternate random-size branch likewise consumes both actual curve pointers. `CreateDecalEffect` receives the resulting size and passes it to `CG_ImpactMark` at0x621f9. Template initialization and the complete default/allocator providers establish live scalar curve storage; neither hunk ownership nor the absence of a reference-count member substitutes for this value-flow proof.

This is the complete consumed pointer identity, not equivalence of the entire constructor with a return-pointer API. Index initialization at0xb9fd3 and scale sampling/store at0x9ffaa–0x9ffaf are separate effects. Scope is valid constructed live scalar curves, valid template/channel storage, disjoint iterator storage and ordinary returning providers. No null/error/malformed-key/hunk-after-release, arbitrary caller replacement, reference-count policy, or entire decal-evaluation contract is claimed. Approved full-width native pointers and hunk ownership remain unchanged. The selected pointer flow has no inherited repair or new defect.

`FxChannel_Copy` remains unchecked because sampled instance scale does not supply both range endpoints; iterator release remains separately unchecked. Independent review accepted this one reference row. No production source or previous resolved verdict changed; no compilation, tests, CI, replay, full build or runtime exercise ran. Private complete original/Mac bodies and source hashes are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-fxreference36.noindex/`. Earlier verification paragraphs above are historical, not checks run for this follow-up.
