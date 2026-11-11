# client/cl_parse_mp module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 5 fixed, 1 intentional.** Complete Mac 1.3 bodies were compared, including both snapshot merge loops and all download/error paths. Call/string triage and constant checks covered every entry. The server-message jump table at 0x33c180 was decoded directly, and all 256 service-name slots were checked. The inactive source is unchanged.

## Corrections

**Gamestate indices used signed checks.** Mac 0x1d56ee and 0x1d5679 use unsigned above comparisons for configstring and baseline indices. Native could admit a negative read result. The checks and the complete bad-command diagnostic at 0x1d55a3 now match Mac.

**Download progress changed at the wrong time.** Mac 0x1d5b27 adds the block size to the legacy UI accumulator before queuing `nextdl`. Native overwrote that accumulator from its connection counter after queuing. The UI update now has the Mac operation order; the connection counter remains native bookkeeping. Mac 0x1d5853–0x1d58a1 also returns for external-URL redirects before changing the UI size. Native updated it first; that update now occurs only on the internal-download path.

**Snapshot ping selected the wrong packet.** Mac 0x1d67e1–0x1d67e7 skips an outgoing packet when its command time is newer than the snapshot command time. Native used the opposite inequality. The search now accepts `p_serverTime <= commandTime`, scans the same 32 slots and keeps the 999 fallback. The entity-section diagnostic at 0x1d5e29 also said `packet clients` in native; it now says `packet entities`.

**Truncated-message diagnostics depended on linker placement.** Mac 0x1d6aa1 indexes the service table with the byte-reader result, including -1 on truncation. The actual preceding Mac word is zero, producing `BAD CMD` before the illegible-message error. Native's relocated pointer table has no declared preceding slot. It now handles the negative result explicitly and follows the same diagnostic/error path.

## Preserved contracts

UDP downloads intentionally remain in native `clientConnection_t`: `CL_BeginDownload` seeds their names/block/count, the parser opens/writes/closes their handle, and `CL_Disconnect` closes that same handle. HTTP redirect state remains in `cls`; local names are mirrored when a download starts and cleared when it ends. The initial suspicion that UDP field placement alone was defective was rejected after reading these complete caller and cleanup contracts. No `cl_main_mp.c` dependency edit was needed or made.

The first-block failure, old/future block and retransmit paths, unsolicited download, open failure, EOF rename, two final packets and next-download calls were compared. Path/URL filtering, safe error formatting and empty-path handling remain native adaptations. The native download backend was preserved.

`CL_SystemInfoChanged` retains protected-cvar filtering and CoD2x policy/IWD updates, including the demo policy reset. Its serverId/demo/cheat/IWD/dedicated/pure sequence and reverse scratch cleanup were compared. The typed gamestate, baselines, snapshots, entity/client rings and outgoing-packet layouts agree with their native callers. Bounded Huffman decompression from WS27, optional diagnostics and the match-summary hooks remain intact.

## Verification and limits

The arm64 Release `cod2_macos_ded` target linked at low priority with two jobs. Because it excludes these client files, both changed client translation units were compiled separately to arm64 objects using the actual client flags, then with CoD2x, network hardening and WWW download enabled. All four passed; the parser retains its existing `strlen` narrowing warning.

Five inactive preprocessing configurations and actual SDK preprocessing with `COD2_X64` removed have no differences. The changed files' active raw-offset inventory has no new violations. `git diff --check` passed. No new tests, full CI or replays were run. The changes are compiled and reviewed, not exercised. No entries remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-network.noindex/`.

## Integrated batch verification

Integrated source `ca4ed64` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
