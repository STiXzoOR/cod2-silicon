# cgame/cg_snapshot_mp module reconciliation

Updated 2026-11-04.

**4/4 reconciled: 2 equal, 1 fixed, 1 intentional.** Read every complete Mac 1.3 body, including `CG_SetNextSnap` routed from `com_vector`, and the snapshot-read and transition blocks inlined into `CG_ProcessSnapshots`.

## Correction

**Snapshot times used a different comparison at integer wrap.** Mac branches on the sign of the wrapped 32-bit subtraction at 0x1dfb02 and 0x1dfb0a when deciding to transition, at 0x1dfada when clamping the current time, and at 0x1dfc9f when rejecting backward server time. Native used signed relational comparisons. A native helper now reads the subtraction's sign bit using defined unsigned arithmetic.

## Native storage and boundaries

Clang's production arm64 record layouts confirm that the raw snapshot fields still have their Mac offsets: entities at 0x26bc, clients at 0x116bc and client-state stride 0x5c. Entity states remain 0xf0 bytes. The native client-info and centity records widen to 1232 and 568 bytes; existing typed indexing and pointer preservation remain in use.

The reset path matches Mac's 0x80000 corpse-clone flag, attachment filtering, animation-tree ownership and player interpolation fields. Initial snapshots preserve listener setup and time initialization. `CG_SetNextSnap` retains native null-import handling for the prediction and synchronous-client dvars; Mac assumes both imports are initialized. The processor's defensive null pending-snapshot gates also remain. Existing cubemap buffer aliasing, inactive-snapshot filtering, restart comparisons and transition continuation order match Mac. No Windows-specific distinction was identified.

## Verification and limits

The arm64 Release dedicated target built at low priority with two jobs. The actual client translation unit compiled separately with its generated production flags. All 25 existing snapshot sanitizer cases passed, covering player-state transitions, corpse copying, normal and cubemap timing, inactive incoming snapshots, restarts and backward times. Integer-wrap endpoints were compiled and reviewed but were not exercised by those cases. No fixture changes, new tests, full CI or replays were added or run.

The five legacy preprocessing configurations are unchanged. Neither changed source uses `__LINE__`. Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-protocol.noindex/`.
