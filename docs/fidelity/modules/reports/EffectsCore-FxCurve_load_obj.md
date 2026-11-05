# EffectsCore/FxCurve_load_obj module reconciliation

Updated 2026-11-05.

**1/2 reconciled: 1 equal; 1 unchecked.** No source change.

The complete allocator at Mac 0xac320 was compared by hand, using a disassembly restarted at the exact symbol to avoid a misdecoded prologue. Allocation, endpoint padding and key copying match. Two unusual details are retained: a padded final key copies values from the first input key, and stored `keyCount` excludes added endpoint keys. The allocator uses the matching pointer-free 8-byte curve header, so native pointer widening requires no adjustment here.

`FxCurve_Reference` remains unchecked. There is no executable Mac identity or native definition/caller; Mac's absolute-zero `FxCurve_AddRef.eh` marker does not identify the Linux pointer-return helper.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.

## Integrated batch verification

Integrated source cef44cb passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

## Allocation identity follow-up

The Reference row remains unchecked. Exact allocation calls
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

The complete original pointer-return body is now anchored at 1.0 0x8121c40 / 1.3 0x81240d0. Its exact actual consumer is FxChannel_Copy: call 0x81236de passes source->curve and stores the returned identical pointer into target->curve. No original counter mutation occurs. This resolves the original pointer contract, but the corresponding complete Mac copy/reference owner remains missing. Hunk ownership, absence of a reference-count field, and a zero-address AddRef exception marker cannot supply that operation. The row remains unchecked.

The retained direct-call scan finds the positive channel-copy-to-curve-reference chain; raw absolute address references add only unwind records. Those bounded searches are not evidence of semantic absence and close no row. No source or prior resolved verdict changed, no new defect was established, and no compilation, tests, CI, replay or runtime exercise ran. Original bodies, disambiguating neighbors, decoded table and fresh exact Mac candidates are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-fx9.noindex/`.
