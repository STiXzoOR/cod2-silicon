# Mac/DirectX 9/CVAOPacket reconciliation

Updated 2026-11-05.

**19/21 reconciled: 11 equal, 7 fixed, 1 intentional; 2 unchecked.** Mac 1.3 i386 is the authority. The legacy OFF bodies are unchanged.

The four derived packet destructors at 0x2ed4c6, 0x2ed4e0, 0x2ed50a, and 0x2ed524 were empty. They now install their derived vtables, call the base destructor, and deallocate in the deleting variants. The pending System c1be798 `CVAOPacket_CopyConstruct` implementation is retained and independently compared with 0x2f10ce: it copies the binding, range words, twelve conventional arrays, and all sixteen generic stream states, retaining the destination's proper vtables.

`InitializeGenericVAO` at 0x1134c4 resets the generic packet without emptying the packet cache. Native incorrectly reset the tree as well. It now preserves the tree. The buffer worker's exact 73b4e6e static empty-tree initializer is retained. Native startup does not invoke the old packet initializer, so the generic packet's GL-free metadata constructor runs lazily on first GL initialization. GL binding creation remains in the existing initialization path. This is the intentional replacement for the load-time metadata work at 0x113ae5–0x113b23.

The callable global destructor at 0x113b40 now uses the generic packet's virtual destructor and erases the tree without resetting its metadata or VAO status, matching 0x113a8e–0x113adc. System already restored tree erasure; this unit refines its direct generic D2 call to the virtual D1 call shown by Mac. The exact System tree eraser is retained and was read against the right-subtree recursion and left-chain destruction at 0x2f1370–0x2f15cf. No automatic exit hook was installed: current-context destruction order is not established. The base constructor/destructor wrappers, status selection, single-packet wrap, and three fixed-function predicates were read against their Mac bodies.

The attributed `DrawPrimitive` and `DrawPrimitiveUP` rows at 0xd6a0/0xd6a8 are genuine zero-return Mac bodies; the actual pending device provider b3accc3 matches them. The fixed and programmable VAO builders at 0xf228/0xfbac are absent from that provider and remain unchecked. Original producers write a buffer-derived pointer and byte count into packet offsets 8/12 (for example 0xf313–0xf340 and 0xfbde–0xfbff). Native packet fields are UINT32, and no widened producer/key/allocation contract is established. The selected device draw instead uses direct client arrays. A cast of those fields to pointers would not restore that contract.

The cache operations are separate ledger ownership: `IsCached`, `ReleaseBuffer`, and `Shutdown` are routed to SDK `allocator.h`, and `Cache` to `stl_pair.h`. Their IDs were not closed. Exact pending System hunks for copy construction, complete generic-pointer copying, Cache, typed tree insertion, insert_equal, and recursive erasure are preserved in the owned source. They are required integration dependencies, not new SDK verdicts from this unit.

Triage and explicit-range constant checks ran. The owned Apple arm64 client object compiles, and the dedicated target builds. Five OFF configurations match with actual line-macro expansion. No tests were added or changed. All corrections are compiled and reviewed, not exercised; the packet metadata constructor is GL-free on the selected GL fallback path.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-arrays.noindex/`.

## Integrated batch 16 verification

Engine d7e6ca9 and fixture-only follow-up 935519b passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
