# qcommon/dobj_management module reconciliation

Updated 2026-11-04.

**14/14 reconciled: 11 equal, 3 fixed.** Mac 1.3 i386 is the authority. The legacy source is unchanged.

Server creation at 0x735be and client creation at 0x737f2 reported a different exhaustion message. Both native paths now use Mac's byte-0x15-prefixed `No free DObjs` string at 0x247434. The Linux-named `Com_ClientDObjCreateLocal` row is the same operation as the Mac client function and shares this correction.

Every body was compared: signed-short map lookup, 1152-slot client indexing and clear loop, null free handling, free-count updates before `DObjFree`, 2048-slot allocation with wraparound from slot 1, model-count and server entity-number truncation to 16 bits, exhaustion check after `DObjCreate`, and initialization/shutdown state. `Com_AllocDObj` corresponds to Linux `Com_GetFreeDObjIndex` and Mac's inlined allocator in both creation functions. The other Linux `Local` rows correspond to Mac's client getter and skeleton-clear function; they were compared there. Native's existing enlarged pool stride accommodates native pointer widths.

String/call triage is clean after the correction. A scripted branch reconstruction confirms `COD2_X64=OFF` source is byte-for-byte unchanged. The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. The correction is compiled and reviewed; no new tests, full CI or replays were run. The changed exhaustion paths have not been exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-map-support.noindex/`.
