# Mac key-resource import reconciliation

Updated 2026-11-05. **3/3 reconciled: 3 intentional.** Source authority is native base `6ae3bd57910d7a82a452422aef218d2b9cf0066d`.

The earlier unmatched-import verdict deferred these rows because a pending `VK_Map` implementation called the zero-returning language helper. The present build, caller and storage graph was checked independently. The imports are retired with their Carbon consumers; the retained helper and CF stubs are not complete native providers.

## Original identities and consumers

`otool -arch i386 -l/-Iv`, `nm -arch i386 -m` and direct file reads establish three five-byte Carbon import slots in `__IMPORT,__jump_table`. Each contains `f4 f4 f4 f4 f4`, not a library implementation.

| Mac slot | Symbol | Indirect-table slot | nlist index |
| --- | --- | --- | --- |
| 0x01acc42e | `CFBundleCopyLocalizedString` | 223 | 413937 |
| 0x01acc429 | `CFStringGetPascalString` | 222 | 413951 |
| 0x01acc424 | `StringToNum` | 221 | 414194 |

The original direct-call inventory was checked against exact-symbol disassembly. `CFBundleCopyLocalizedString` has 15 calls in eight MacResources bodies and `MacTools::MessageAlertFromKey` at 0xaa9e. These cover game strings, product-family lookup, the language cache and startup error strings, including inlined product/data-folder lookups. `CFStringGetPascalString` has two direct calls: `GetGameString` at 0x9c54 and `GetLanguageCode` at 0x9d2a. `StringToNum` has one, at 0x9d42.

Mac `GetLanguageCode` at 0x9cbc tests its separate cached integer against 0x7fff, obtains a localized CF string, converts it into a 256-byte Pascal buffer with encoding zero, releases the CF value and passes the buffer/cache address to `StringToNum`. The conversion result is not tested. `TranslateKeyCodeToVK` at 0xccd4 uses language 1 for French, queries again for German language 2, otherwise selects ANSI, then caches the table. Original key-up/down bodies at 0x10b912/0x10bb1e call this translator. No external CF or Carbon implementation was inferred from those call sites.

## Selected native boundary

`CMakeLists.txt` selects `cmake/macos-arm64.cmake` for Apple `COD2_X64`. That source list retains `MacResources.c`, `VK_Map.c`, `MacTools.c` and client `mac_input.c`, excludes legacy `MacDisplay.c`, and explicitly selects `src/unix/linux_input.c` for the client. Mere compilation does not make the old resource graph live.

The only incoming native source calls to `TranslateKeyCodeToVK` are the retained `CCallOfDutyEngine_DoKeyDown`/`DoKeyUp` bodies. No source registers or calls those callbacks. The only external source calls to `MacResources_GetLanguageCode` are inside that translator. Other MacResources names have no incoming source calls; the excluded MacDisplay file only declares `MacResources_GetProductFamily`. `MacTools_MessageAlertFromKey` and the retained CF alert wrapper also have no incoming source or generated-data references.

The live path is `Sys_GetEvent` in `Mac/Main/mac_main.c` -> `IN_Frame` -> `SDL_PumpInputEvents` in `unix/linux_input.c`. On Apple `COD2_X64`, it uses `SDL_PumpEvents`/`SDL_PeepEvents`; key-down/up map `ev.key.keysym.sym` with `sdl_to_keynum` and queue `SE_KEY` directly. `SDL_TEXTINPUT` decodes UTF-8 and queues `SE_CHAR` directly. Neither path calls the Carbon callbacks, the translator or a MacResources helper. The dedicated source list excludes `mac_input.c` and does not select `linux_input.c`; its native `Sys_GetEvent` does not call `IN_Frame`.

The selected typed-data payload is the four `*_native.c` files from `cmake/datagen.cmake`, either the committed `build/lp64_gen` snapshot or regenerated files verified against it. Those four snapshot files contain no MacResources, translator or key-callback references. `typed_types.h` retains only an unused MacResources extern, `dg_ref_323`; no initializer uses it. The `.long MacResources_GetMissingDataFolderError+135` in `src/blobs/data.S` is inside the old scalar `virtualKeyConvert` table, not a callback registration. That assembly blob is not in the native source set; the table is defined as bytes by `mac_input.c`. Retained `sCachedVKMap` storage does not install an event callback.

Engine localization uses `loc_language` in `PC/stringed/stringed_hooks.c`; native error prompts use `Win_LocalizeRef`/`localization.txt` and the SDL/stderr `MessageBoxA` provider in `MacWin32.c`. These paths do not consume the old GameStrings CF cache or Pascal buffer. This closes the three import boundaries as intentional native platform replacements, not equivalent external-library functions.

## Verification and limits

No production source changed. Verification is the original import/consumer inspection and current source/build/storage proof above; `git diff --check` and exact three-ID ledger validation passed. No compile, new tests/cases/assertions, full CI, replay or game launch was run. Source proof does not establish a runtime keyboard-layout result.

`MacResources_GetLanguageCode` still returns zero; `link_stubs.c` still defines zero-returning `CFBundleCopyLocalizedString`/`CFStringGetPascalString`, the CF calls in MacResources still use integer arguments/returns, and `StringToNum` has no native call. None is approved for a future live caller. Restoring such a caller requires a new provider/storage audit. French/German resource detection, keyboard-layout parity and original resource/error-text parity are not claimed. No assigned rows remain unchecked.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-key-resource-imports.noindex/`.
