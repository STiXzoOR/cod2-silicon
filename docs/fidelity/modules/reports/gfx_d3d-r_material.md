# gfx_d3d/r_material module reconciliation

Updated 2026-11-05.

**30/30 reconciled: 21 equal, 6 fixed, 3 intentional.** Mac 1.3 i386 is the authority. Current Apple GL is the selected renderer. Legacy OFF source is unchanged.

## Corrections

`Material_Duplicate` now retains the destination name when replacing an existing entry, as Mac 0xd3ee7/0xd3f00 does. The previous copy replaced the name with the source material's name.

`R_RegisterRawImage` now resolves the global registry before handling an empty name. Mac 0xd3634 returns the default material; native dereferenced an uninitialized local pointer.

Both vertex-declaration builders now move the last element into the new slot before walking backward. Native began one slot early. Mac 0xd37e2 and 0xd3b3f establish the insertion order. `Material_ReloadAll` also visits shader slot 255, which Mac 0xd39b5 includes, and retains the complete device-recovery diagnostic.

`Material_Init` copies the full native texture-name pointer into `rawTexdef`, following the field copied by Mac 0xd4198. `R_InitImages` has already initialized its sampler state and raw image. The old four-byte write truncated an LP64 pointer. Raw-material and texture-reload command diagnostics now match the binary.

## Comparisons and native boundaries

The registry hashes, collision probes, limits, shader/literal comparisons, default identity, release/reset loops, picmip traversal and texture reload order were read against their Mac bodies. The original missing literal-count increment is preserved. A binary-data comparison checked all 28 built-in and 12 fill names, 84 stream-source bytes and 24 destination bytes.

Native keeps full-width technique-pointer ordering instead of a wrapped i386 pointer subtraction, the opt-in allocation-canary probe in registration, and the external-renderer/null-name registration adapter. These are the three intentional entries.

The actual image/font source at 54f9bd3, System source at c1be798 and device source at b3accc3 were inspected. The System diff from d26c2d3 adds render-thread synchronization and pointer-count sorting thresholds to `Material_Sort`; this unit leaves those provider-owned hunks for integration before this commit. Native device declaration/shader creation remains the GL provider.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built with two low-priority jobs. Both changed Apple client objects compiled separately with their generated compile commands. Ten inactive-source comparisons passed against f0d941a; neither source has `__LINE__` diagnostic sites. `git diff --check` passed.

No new tests, full CI or replays were run. These corrections were compiled and reviewed, not exercised. The valid stream tables use stream zero, so the declaration insertion correction needs a suitable runtime caller to exercise that branch.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-materials.noindex/`.

## Integrated batch 16 verification

Engine d7e6ca9 and fixture-only follow-up 935519b passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
