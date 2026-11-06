# EffectsCore/FxMemMgr module reconciliation

Updated 2026-11-05.

**5/5 reconciled: 1 equal; 4 intentional.** No source change.

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

## Original pool consumers and selected Mac backend

New original executable provenance identifies all four block helpers inside complete typed-manager lifetimes. Original1.3 FxBoltFrame Acquire0x8124386 calls operator new0x8129b74, which reaches manager Alloc0x812a0da. Its call0x812a149 reaches AllocBlock0x812411a; the alternate block-promotion call reaches MoveToFront0x812a1b8. Release0x8124442 reaches operator delete0x8129b90 and manager Free0x812a200, which calls FreeItem0x81242b2 at0x812a214 and can call FreeBlock0x8124292 at0x812a291. MoveToFront and FreeBlock both invoke UnlinkBlock0x812422a. Complete original1.0 bodies corroborate the identities.

These original owners preserve the keyed bolt-frame list and reference count while allocating from and returning to the32KB block arena. Complete Mac Acquire0xa105a and Release0x9fe6c preserve that object lifetime through direct array-new/conditional zeroing and array-delete. The previously established original Mac STABS/SLINE describes the actual fieldless FxMemMgr specializations and their Alloc/Free implementation. Native uses those complete selected owners with native object sizes. This positive original consumer-to-Mac backend mapping closes the four pool helpers as intentional existing backend replacement.

The original pool address/reuse rules,2MB capacity, failure path, free counts, high-water accounting and links are not equal or newly reconstructed. No standalone arbitrary FxMemBlock API is claimed. This conclusion uses complete original and Mac owners and positive typed-manager metadata, rather than absent symbols, callers or arena fields. The existing native allocator choice and all settled allocation-wrapper verdicts remain intact. Independent review accepted the four helpers and the related MoveToFront member. No source change, compilation, tests, CI, replay or runtime exercise occurred. Private exact bodies and caller evidence are retained in recon-fxbackend7.noindex.
