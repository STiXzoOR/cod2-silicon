# EffectsCore/FxScheduler_load_obj module reconciliation

Updated 2026-11-05.

**17/17 reconciled: 11 equal, 4 fixed, 2 intentional.** Mac 1.3 i386 is the authority. All seven Mac bodies and the ten identified inline helper rows were read. Type names, diagnostics and fixed constants were checked directly against the binary. The legacy source bodies are unchanged.

## Corrections

**Effect files used the wrong file-opening API.** Mac 0x5d250 calls `FS_FOpenFileByMode` with `FS_READ`. Native called `FS_FOpenFileRead` with a shared handle. The native mode wrapper also marks file access, applies pure-ignore lookup, requests a unique handle and initializes stream metadata. The X64 loader now follows the Mac entry point. The existing parser fixture's file-open adapter was renamed and checks `FS_READ`; its test cases are unchanged.

**Pointer-containing effect storage needs native alignment.** Mac 0x5d2b8 allocates the 0x68-byte effect template aligned to four. Native already used `sizeof(EffectTemplate)` but still requested four-byte alignment. It now requests `_Alignof(EffectTemplate)`. `Hunk_AllocAlignInternal` rounds the high hunk pointer by that alignment and zeroes the requested size, establishing defined initial count and primitive slots. Strings remain aligned to four.

**Native allocation sizing was gated only by the Apple SDK.** Media growth at 0x5d182 and 0x5d67a allocates and copies four-byte Mac entries; native entries contain full pointers. The existing pointer-sized Apple branch now also applies to every `COD2_X64` target. Primitive-template allocation likewise uses its native size and alignment for all X64 targets. Initial capacity four, 16-bit doubling, append order, copy/free ordering and count updates are preserved.

## Native contracts and retained differences

The twelve type names map to the same values. Unknown groups still advance the parent index. The 24-primitive cap, failed-parse cleanup, invalid-material cleanup, extension stripping, lowercase registration, one leading separator removal and default effect fallback match Mac 0x5d218 and 0x5d5fa. The original over-capacity primitive is not separately freed.

Client material validation reads the renderer-exists byte: `fx_developer_check_ptr` aliases `imp_g_rendererExists`. Dedicated builds compile out client material validation and media registration; their renderer is absent. This existing headless adaptation remains. Native object and pointer sizes replace i386 byte counts; no shared-header changes were needed.

## Verification and limits

Configured an isolated arm64 Release build and linked `cod2_macos_ded` at low priority with two jobs. Both assigned translation units separately compiled using the actual local client and dedicated compilation commands. Existing dedicated and client parser fixtures passed under ASan/UBSan, exercising registration, allocation, parser integration, typed media-list growth, cleanup, bolt reference handling and cache reuse. No new cases or tests, full CI, replays, stock-data audit, or integration builds were run.

File-system access/purity/stream metadata are represented by the existing fixture adapter, so the real file-system side effects were compiled and reviewed but not exercised. The hunk fixture allocates through zeroed `malloc`, so real high-hunk alignment behavior was reviewed in the allocator rather than run. No non-Apple binary was built. Error diagnostics, over-capacity primitive handling and default-file failure were not exercised. The OFF guard checker reported ten comparisons and zero mismatches across the two changed source files; neither file uses `__LINE__`.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-scheduler.noindex/`. Per-ID verdicts are in the private `fx-scheduler.ledger.json` supplied to the orchestrator.
