# win32/cinematics module reconciliation

Updated 2026-11-05.

**15/16 reconciled: 8 equal, 4 fixed, 3 intentional; 1 unchecked.** Exact-symbol Mac 1.3 disassembly is the primary reference. Windows 1.3 was checked for the distributor-logo and pixel-layout differences. All changes use `COD2_X64`; inactive source bodies are unchanged.

## Corrections

`ROQ_PlayCinematic` now stores the selected `cin.currentHandle` (Mac 0xb20d2) and starts with `status = FMV_PLAY` (0xb24b8). Native left the cached handle at -1 and wrote `letterBox` instead of the playback state. The failed-open path now invokes the next-movie helper (0xb25d8), and the exhaustion diagnostic matches cstring 0x249f00. Extra initial VQ0/VQ1 and motion-byte writes were removed. The green-chroma coefficient bits now match 0xb22c0/0xb22c8; a float32 script found no resulting integer-table changes across all 256 entries.

`RoQReset` no longer writes motion bytes that Mac 0xb2b1d leaves untouched until the next packet header. Its stream lifecycle, header, clock, FPS and LOOPED-state writes were compared. `ROQ_RunCinematic` again only validates its handle and calls the runner, as at 0xb5d24; EOF cleanup belongs inside the runner.

`blitVQQuad32fs` now sign-extends the stored 32-bit motion displacement before LP64 pointer addition. Mac adds at 0xb1ab4/0xb1f7f wrap at 32 bits. Native had zero-extended negative previous-buffer displacements into offsets near 4 GiB. All codeword, blit/copy, stride, quadrant and sentinel paths were compared by hand.

The runner received corrections for its normal four-byte path: the raw-audio clock and signed frame calculation (0xb3f70), SSE integer-conversion behavior for material clock adjustment (0xb2d83/0xb4977), continued frame processing and timeout handling (0xb2e25/0xb300e/0xb3a7d), hold/loop/EOF cleanup (0xb36ba/0xb3fb5/0xb3f39), info clocks/counts (0xb335e..0xb3d68), and codebook low-byte-zero counts (0xb3a44). The jump table at 0x32a560 was decoded directly: 0x1012 is a no-op, 0x1013 clears chunk size, and 0x1030 starts in-memory subchunks. Header validation, subchunk iteration and played-byte update order now follow 0xb3137..0xb375a.

## Intentional boundaries

The new `RoQ_ExecAnySubsequentMovies` helper follows Windows 0x4b2350..0x4b2386 and its failed-open chain at 0x4b2910..0x4b2933: queue `nextmap`, clear it, open menu 1. Mac 0xb14f2 inserts an Aspyr distributor movie before `IW_logo` once. Native follows the requested Windows player-visible behavior. The same helper is used by shutdown at the 0xb1742 boundary; the surrounding shutdown guards, teardown, game-state restoration and slot clearing match Mac.

The engine ROQ decoder remains linked into the native client. Its draw wrapper forwards the same aspect, clear, raw-frame and letterbox arguments as Mac 0xb25e8. The retained D3D raw-texture path creates format 22 (`RB_StretchRawCmd`), which the Classic format provider consumes as BGRA and Metal's `resource_formats.m` converts from B,G,R,A. Native therefore keeps B,G,R,255 codebook bytes. Mac 0xb5566 packs FF,R,G,B; Windows helper 0x4b0a10..0x4b0a8f packs R,G,B,FF. This is a storage adaptation at the renderer boundary, not a claim that pixel bytes match either original backend.

After successful EOF teardown, native returns `FMV_IDLE` instead of reproducing Mac's subsequent `cinTable[-1]` read. If shutdown declines because there is no decoded buffer, native retains the EOF result. `ROQ_PlayCinematic_f` also stops its initial-buffer polling when shutdown cleared the current handle, instead of reproducing the next negative-index read at Mac 0xb5e4c. Its remaining argument/state/sound/polling paths were compared. No undefined native storage was introduced to emulate the negative index.

The remaining equal rows were compared by hand: close-all, draw-global, set-extents, stop-global, stop-handle, upload-state, recursive quad setup and draw-handle. For example, upload is only the play-on-walls/dirty-state machine in Mac 0xb1856 too; there is no omitted renderer upload call.

## Remaining runner audit

`ROQ_RunCinematicFromHandle` remains **unchecked**. The earlier batch left its dormant grayscale, RGB565, half-size and smoothed codebook branches incomplete. The follow-up reconstructs them from Mac 0xb33ec..0xb5d1f: normal cells are 2x2, half-size cells select luminance bytes 0 and 2 for 1x2 cells, and smoothed cells are 2x4 with the original integer 3:1 and 1:3 vertical luminance interpolation. All modes consume six input bytes per base cell and two cell indices per expanded strip; horizontal and vertical duplication populate the larger codebooks. RGB565 uses the original 9/8/9 shifts and 31/63/31 clamps. Four-byte modes retain the documented native BGRA adaptation. No native creator supplies a grayscale map, so the null-map state preserves its previous defined no-op; a supplied map uses the Mac lookup order.

The input allocation is now established. Mac 0xb3020 addresses `cin.file` at 0x7b05c0 and 0xb30a3 addresses `cin.sqrTable` at 0x7c05c0. A 65,536-byte payload plus the following eight-byte header overlaps the first eight square-table bytes inside the enclosing global `cin`; it does not exceed that allocation. Native retains the same adjacency and uses a byte pointer to the enclosing object representation, preserving the overlap without indexing past the declared file member. The first packet size is now checked before reading, because the creator's initial header has not passed the later Mac 0xb31a4 size check.

Mac reserves 65,536 decoded-audio bytes at 0xb2c88. Its mono loop at 0xb3093..0xb30ba writes four bytes per compressed byte, and its stereo loop at 0xb32bb..0xb32ee writes two bytes per compressed byte rounded up to an even count. Native now owns 262,144 decoded bytes, enough for every size accepted by the 65,536-byte chunk limit. This defines native output where the original overruns its stack; it does not claim to reproduce corrupted-stack behavior.

Unchecked scope is now narrower: malformed in-memory subpacket chains can escape the enclosing input object; malformed dimensions can overflow the image/quad/sentinel storage contract; and malformed VQ codewords or motion references lack a proved input/output extent contract. Those paths still need either complete bounded native rejection preserving defined original streams or a complete original layout proof. This report does not certify the entire runner or malformed-stream handling.

## Native ownership and build matrix

`src/PC/client_mp/cl_cin_mp.c` is the sole external caller surface. Apple SDK clients return -1 from `CIN_PlayCinematic` and finish the cinematic command immediately, while their run/draw wrappers remain linked; the Apple dedicated source graph excludes both client code and `cinematics.c`. The Linux, MinGW and MSVC client graphs retain `cinematics.c` and their public creators call ROQ. The shared Linux dedicated object graph also compiles it, without a dedicated playback entry. Neither the Apple wrapper replacement nor the original backend's exclusion can exempt this shared engine decoder.

All source references to `cin_cache`, `cinTable` and the decoder's mode fields were checked. `ROQ_PlayCinematic` is the only bytes-per-pixel writer and selects four; it installs `blitVQQuad32fs` as both normal/buffer callbacks. The runner info packet is the only writer of half/smoothed mode and clears both. No native grayscale-map owner exists. The dormant variants therefore have no supported creator path in any current build, but their code is now reconstructed rather than used as a reason to exclude the entire function.

The follow-up compiled the changed `cinematics.c` using both actual arm64 Release client compile commands (stock and CoD2x), remapped to the isolated checkout, with background scheduling, niceness 19 and two compiler jobs at most. Both succeeded. The guard check confirms all five inactive source configurations are identical, and the source has no `__LINE__`. No test or fixture was added or changed, and no full CI, replay, full client link or decoder playback was run. These changes are compiled and statically reviewed only. Follow-up private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-roq-remaining.noindex/`.

## Verification

The arm64 Release `cod2_macos_ded` target built with background scheduling and two jobs. Because it excludes cinematics, the actual client compile commands were also used to compile `cinematics.c`, `win_voice.c` and `win_localize.c` separately; all succeeded. The changed production files contain no `__LINE__`. The guard checker reports only the two expected native-on differences, with no inactive-configuration differences. `git diff --check` passes.

No tests were added or changed, and no CI matrix, replay, complete client link or runtime exercise was run by this unit. Corrections are compiled and reviewed only. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-platform-media.noindex/`.

## Independent review and reachability

Independent source/binary review found no introduced mismatch. It confirmed one changed failure behavior: after failed sound initialization, an audio-bearing ROQ stream follows the reference raw-audio clock of zero and can stop at the watchdog instead of continuing silently using wall time. Both Mac and Windows do this. The earlier native fallback dates to the initial reconstruction, with no documented intentional replacement contract found; the reconciliation retains the reference behavior under the explicit rule to preserve deterministic original quirks.

This does not enable Apple product-path cinematic playback. The public CIN_PlayCinematic and CL_PlayCinematic command wrappers still implement the documented WS11 immediate-finish boundary in client_mp/cl_cin_mp.c. The normal four-byte decoder path above describes what happens when ROQ is called directly; no runtime reachability or complete decoder certification is claimed. The negative-index guards and unresolved malformed-packet limits remain as stated above.

## Integrated batch verification

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.
