# Mac/Tools/MacPreferences reconciliation

Updated 2026-11-05. **9/9 reconciled: 6 equal, 1 fixed, 2 intentional.**

Mac 0x966c calls `CFStringGetCString` with the caller's capacity; failure releases the preference and follows the default/empty path, returning false. Native file preferences previously truncated oversized stored values and returned true. Native Apple reads now close the file and take that same fallback when the value and terminator do not fit. Live `Com_ReadCDKey` uses a 21-byte buffer, so this is a reachable contract. The correction is compiled and reviewed, not exercised.

The six numeric/rectangle wrappers match Mac's ordered suffixes, decimal formatting, byte/default behavior, empty-value branches and `atoi`/`atof` conversions. Project `CGRect` remains four floats; no SDK double rectangle is passed here. Mac rectangle wrappers are void; the extra native success byte has no caller.

`PutString` and `Synchronize` intentionally retain the WS21 shared file store, `~/.cod2/preferences`, used by the launcher and engine CD-key functions. Writes close the mode-0600 temporary file before rename; synchronization has no CF store to flush. This is not a claim of fsync crash durability. The CFPreferences backend is intentionally replaced.

Only Apple `COD2_X64` code changed. The six inactive preprocessor configurations compare identically with base 3d9df23; neither changed file contains `__LINE__`.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.
