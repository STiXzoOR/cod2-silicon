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

The actual image/font source at 84ba122, System source at 5f88bd5 and device source at 9f05cde were inspected. The System diff from 02a2156 adds render-thread synchronization and pointer-count sorting thresholds to `Material_Sort`; this unit leaves those provider-owned hunks for integration before this commit. Native device declaration/shader creation remains the GL provider.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built with two low-priority jobs. Both changed Apple client objects compiled separately with their generated compile commands. Ten inactive-source comparisons passed against 28b9601; neither source has `__LINE__` diagnostic sites. `git diff --check` passed.

No new tests, full CI or replays were run. These corrections were compiled and reviewed, not exercised. The valid stream tables use stream zero, so the declaration insertion correction needs a suitable runtime caller to exercise that branch.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-materials.noindex/`.
