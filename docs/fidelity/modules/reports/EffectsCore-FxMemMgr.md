# EffectsCore/FxMemMgr module reconciliation

Updated 2026-11-05.

**1/5 reconciled: 1 equal; 4 unchecked.** No source change.

The exact-symbol Mac `FxMem_Init` entry at 0xacbd8 is a no-op through its return at 0xacbdc, matching native. The apparent later instructions are padding and the next routine, not arena initialization.

`FxMem_AllocBlock`, `FxMem_UnlinkBlock`, `FxMem_FreeBlock` and `FxMem_FreeItem` remain unchecked. No executable Mac identity or identified inline body was found for those Linux names. Native has no definitions, callers, `FxMemBlock` layout or `fx_memBlocks` arena. Existing effects allocations use `__Znam`/`__ZdaPv` paths with native object sizes. The Linux 32KB arena, 32-bit link words and masked pointer-to-block lookup were not introduced without a matching Mac allocator/layout contract. No shared headers were edited.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.

## Integrated batch verification

Integrated source cef44cb passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

## Allocation identity follow-up

The four block helpers remain unchecked. Mac STABS now positively identifies
the live FxMemMgr specializations as one-byte, fieldless classes; source-line
records tie their allocation/free behavior to direct array-new, zeroing and
array-delete. This settles the separate primitive allocation-wrapper identities,
but provides no FxMemBlock layout, arena or executable block-helper mapping.
The FxMem_Init verdict is preserved exactly.

No production source changed. The assigned sources compiled with exact stock
and CoD2x arm64 Release commands. Both existing primitive ASan/UBSan fixtures
passed, using their calloc/free allocation adapters; they do not exercise these
unresolved helper identities or the production C++ allocator. No new tests,
cases, assertions, full CI or replays were run. See the FxPrimitives follow-up
for the shared ownership evidence and verification limits.
