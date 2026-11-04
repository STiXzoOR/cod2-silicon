# cgame/cg_localents_mp module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 3 equal, 3 fixed.** Compared full Mac bodies, including both compiler initializer rows. Legacy source is unchanged under COD2_X64=OFF.

## Corrections

The shared free-list error omitted the 0x15 prefix used by Mac 0x1e009d and 0x1e0123. Both allocation exhaustion and expiration now emit the original diagnostic.

Mac MINSS at 0x1dff98 selects tracerLength on equal or unordered operands. Replaced the native greater-than clamp with the corresponding selection; normal finite tracer arithmetic keeps its existing operation order.

## Storage and comparison examples

The actual generated data relocations already bind cg_localEntities to the 128-object typed pool and cg_activeLocalEntities to the typed sentinel. The unused raw data.c definitions are not the client build contract. No pointer initialization workaround was added. Native objects are 208 bytes; typed clearing and links replace the original 188-byte stride.

Compared reverse traversal, oldest-entry eviction, trajectory-time expiration, list insertion and all tracer dot-product/endpoint arithmetic. The initializer name repeats in the Mac symbol table, so the 0x1dffde body was checked by address. Its guarded copy reads the initially-null per-client free slot; native C zero initialization supplies that same startup state.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built at low priority with two jobs. It excludes cgame, so all five assigned client translation units were also compiled individually with this worktree's actual `cod2_macos` compile commands. OFF preprocessing matches the base with real diagnostic line expansions preserved. No new tests, full CI or replays were run. Changes are compiled and reviewed, not exercised in a running client.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-world.noindex/`.
