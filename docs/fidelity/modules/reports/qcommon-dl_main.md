# qcommon/dl_main module reconciliation

Updated 2026-11-04.

**11 entries reviewed: 2 equal, 1 fixed, 7 intentional, 1 unchecked.** All eleven Mac bodies were read from exact-symbol disassembly. Triage and constant checks also ran. The established native SDK libcurl replacement is retained; the original libwww-specific callbacks and request objects are not reconstructed.

Mac `HTAlertCallback_progress` writes transferred byte counts to `LegacyHacks.cl_downloadCount` at 0x1fb9c1 for HTTP and 0x1fb9f3 for FTP. The native write callback previously never updated that field. It now accumulates bytes written to the destination, updates the same UI field, and returns a byte count. This correction is compiled and reviewed, not exercised with a transfer. Error and short-write timing equivalence to libwww is not claimed.

`DL_BeginDownload` also had two established omissions. Native guessed MOTD status from the URL substring `motd.txt`; the only Mac text reference to that flag is its byte load at 0x1fbc39. The heuristic is removed. Mac 0x1fbffc–0x1fc009 fills the 64-byte display name with a URL whose authority credentials become `*:*`, and 0x1fc087–0x1fc094 displays URLs without credentials. The native path now fills that UI field and uses the redacted value in its diagnostic. Ordinary absolute URLs follow the observed scheme/authority/path format.

**`DL_BeginDownload` remains unchecked.** The exact libwww `HTParse` normalization and escaping of unusual credential-bearing, relative or malformed URLs was not established, and the bounded native display parser does not claim to reproduce it. The original no-credentials branch also passes the remote URL as a format string, while the native display copies it as text. HTTP Basic Auth parsing, timeout semantics and failure mapping remain part of the intentional curl replacement, with no request-level equivalence claim. No credentials or live download data were used as evidence.

The two query functions retain the Mac running-positive and MOTD-state semantics. Initialization, cancellation, event pumping, completion status, diagnostic callbacks, redirect confirmation and prompt refusal are implemented by the native curl/status/teardown interface. Their ledger entries explicitly describe this intentional boundary rather than claiming equal library behavior.

No shared headers were changed. `unifdef -UCOD2_X64` output is byte-identical to base e92ca5f02da88f521838bb2de89d1563524f5ba3. `cod2_macos_ded` compiled this translation unit in the worker's arm64 Release build with `taskpolicy -b nice -n 19` and two jobs; dedicated linking does not exercise this client subsystem. No new tests, HTTP transfers, full CI or replays were run. The one unchecked row and the transfer-level limits above remain for follow-up.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-net.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
