# universal/com_file_wrapper module reconciliation

Updated 2026-11-04.

**5/5 reconciled: 5 equal.** The engine wrappers at Mac 0x373d8, 0x373e2, 0x373ec, 0x373f6 and 0x37400 forward directly to `fread`, `fwrite`, `fopen`, `fclose` and `fseek`. Native `src/PC/qcommon/com_fileaccess.c` forwards the same arguments and returns using the host stdio types. These engine wrappers are in scope; upstream stdio implementations are not.

The `FS_FileRead` prologue was rechecked with exact-symbol disassembly because full-binary disassembly misaligned its entry. No wrapper changes were needed. The arm64 Release dedicated target compiled; no runtime or replay checks were run.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-files.noindex/`.
