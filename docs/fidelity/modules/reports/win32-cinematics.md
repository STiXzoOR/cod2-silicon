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

## Unresolved runner coverage

`ROQ_RunCinematicFromHandle` remains **unchecked**. Its dormant grayscale, 16-bit, half-size and smoothed codebook variants are incomplete and were not reconstructed speculatively. Writer searches show normal native playback selects four bytes per pixel and info processing clears half/smoothed mode. That does not prove the entire original function equal.

Malformed chunks can also exceed the 65,536-byte input area or decoded-audio stack contract. Mac reserves 65,536 decoded-audio bytes at 0xb2c88; native now uses that capacity, but the maximum chunk-size check alone does not bound mono expansion. Those malformed-input/layout contracts are unresolved. No runtime claim is made for any runner correction.

## Verification

The arm64 Release `cod2_macos_ded` target built with background scheduling and two jobs. Because it excludes cinematics, the actual client compile commands were also used to compile `cinematics.c`, `win_voice.c` and `win_localize.c` separately; all succeeded. The changed production files contain no `__LINE__`. The guard checker reports only the two expected native-on differences, with no inactive-configuration differences. `git diff --check` passes.

No tests were added or changed, and no CI matrix, replay, complete client link or runtime exercise was run by this unit. Corrections are compiled and reviewed only. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-platform-media.noindex/`.

## Independent review and reachability

Independent source/binary review found no introduced mismatch. It confirmed one changed failure behavior: after failed sound initialization, an audio-bearing ROQ stream follows the reference raw-audio clock of zero and can stop at the watchdog instead of continuing silently using wall time. Both Mac and Windows do this. The earlier native fallback dates to the initial reconstruction, with no documented intentional replacement contract found; the reconciliation retains the reference behavior under the explicit rule to preserve deterministic original quirks.

This does not enable Apple product-path cinematic playback. The public CIN_PlayCinematic and CL_PlayCinematic command wrappers still implement the documented WS11 immediate-finish boundary in client_mp/cl_cin_mp.c. The normal four-byte decoder path above describes what happens when ROQ is called directly; no runtime reachability or complete decoder certification is claimed. The negative-index guards and unresolved malformed-packet limits remain as stated above.
