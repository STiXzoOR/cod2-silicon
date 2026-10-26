# F2 command scheduling and download cleanup

2026-10-26; base `f494bf9`; native stock and CoD2x.

Client packets now use the original minimum intervals: 50 ms while a download
temporary name is present, and 1,000 ms before the primed/active states otherwise.
The elapsed time comes from the last packet actually sent. Previously the
scheduler used connection retry time and mistook demo-record startup state for
a download, so it could send too frequently during connection/downloads and
unnecessarily delay packets when recording began. Local/LAN shortcuts now run
after these checks and inspect the channel's remote address.

The corrected predicate needs accurate download lifetime. UDP completion and
rejection clear the static name mirrors; completion after a filesystem restart
and ordinary disconnect clear static download state through the existing helper.
A detached WWW download survives disconnect, as in the original. Static names
are populated in native 1.3 even when the optional WWW feature is disabled.

## Evidence and scope

The owned Mac MP 1.3 assembly is the authority. Its instruction bytes were
checked against the binary for the nine relevant scheduler/packet/download
bodies (1,260 instructions, zero mismatches); pseudocode was only a reading aid.

- `CL_SendCmdInternal`, `0x18a032`: `0x18a072` loads the byte at
  `cls + 0x2b4838`; `0x18a089–0x18a097` and `0x18a0e0–0x18a0ec` subtract
  `clc + 0xc` from realtime and compare against 999 and 49. STABS identifies
  these fields as `downloadTempName[0]` and `lastPacketSentTime`.
  `0x18a099–0x18a0c8` uses `netchan.remoteAddress` for loopback/LAN detection.
  `CL_WritePacket` already updates the last-send time and packet history.
- `CL_ParseDownload`, `0x1d59c8`: the EOF path at `0x1d5b8b–0x1d5b92`
  clears both static names before its two direct packet writes and next-file
  transition. The native UDP parser still owns its file handle and legacy
  connection fields; this repair clears their static mirrors alongside them.
  Existing optional invalid-name rejection likewise clears both representations.
- `CL_DownloadsComplete`, `0x14d368`: `0x14d54b–0x14d553` suppresses the
  `donedl` reliable command when WWW disconnected; `0x14d5a6–0x14d5e3`
  clears static state and cancels the downloader after filesystem restart.
- `CL_Disconnect`, `0x14b8ea`: the normal-download condition at `0x14b913`
  and cleanup at `0x14bac9–0x14bb06` preserve detached WWW activity and clear
  ordinary download state. The native repair calls the existing equivalent
  `CL_ClearStaticDownload` after connection reset, before sound disconnect.

This also corrects the previous client-input report: STABS gives
`downloadName` at bit 22,690,240 / 8 = `0x2b4738`, and `downloadTempName`
at bit 22,692,288 / 8 = `0x2b4838`. The earlier report named the first field
at the second field's address. The new fixture distinguishes the two names.

The channel-address change matches the original selection. Normal connect
responses usually retain the same address class as the challenge address;
the fixture tests different classes synthetically, without claiming a reproduced
live LAN failure. This unit does not reconcile every client download field or
all of `cl_main_mp.c` and `cl_parse_mp.c`.

## Verification

`tests/input/run_scheduling.py` selects complete active production bodies and
uses client engine flags with assertions, ASan and UBSan. It includes the real
scheduler, packet writer, begin/next/completion/disconnect functions, UDP parser,
and WWW redirect/completion functions. Synthetic boundaries provide message
encoding/reading, command creation, filesystem/HTTP operations, LAN classification
and final transport. In particular, last-send time and packet-history updates
come from the production packet writer, not a synthetic scheduler result.
Unrelated initialization paths abort if entered. The runner also compiles all
three changed translation units with these flags.

Both default builds pass nine groups covering:

- Actual download producers; 49/50 ms and repeated-send boundaries; distinction
  between destination and temporary names.
- Connected/loading 999/1,000 ms boundaries and local/LAN gate order.
- Active max-packet boundaries, command generation while throttled, demo-record
  waiting, demo playback, cinematic and logo controls.
- Channel endpoint selection and real packet-history updates.
- UDP data/EOF, failed file creation, filesystem restart and static cleanup.
- WWW completion, normal disconnect, detached-download preservation, and
  suppression of `donedl` while detached.

The unchanged `--baseline f494bf9` fails eight groups (29 assertions) in both
builds; the ordinary active/demo controls pass. These are intended behavioral
failures. Both builds also pass with `--net-hardening`, exercising optional
invalid-name cleanup, and with `--no-www`, exercising UDP-only name production
and cleanup. The latter uses the header's real disabled downloader helpers and
skips HTTP execution; its remaining nine groups include disconnect checks.

```sh
# Run serially with each matching stock/CoD2x engine database.
taskpolicy -b nice -n 19 python3 tests/input/run_scheduling.py \
  --compile-db <build>/compile_commands.engine.json --output <private-output>
# Use a separate output directory for --baseline f494bf9, --net-hardening,
# and --no-www variants.
python3 tests/fixes13/legacy.py --base f494bf9
python3 tools/abi/legacy.py --base f494bf9 \
  --compile-commands <private-remapped-engine-database>
```

All three source files are byte-identical with native code removed by
`unifdef -UCOD2_X64 -U__aarch64__ -U__arm64__`. Legacy checks pass ten plus
five inactive configurations, and three actual SDK preprocessing commands with
`COD2_X64` removed. The SDK check used one worker and a private database remapped
to this worktree. Its first attempt failed because the database's build-directory
cwd did not exist; correcting that private cwd produced the passing result.

The first no-WWW fixture attempts failed to compile because test downloader
stubs redefined the header's disabled inline helpers. The fixture now retains
those actual helpers and makes HTTP checks conditional. Initial failure statuses
remain in `results.json`; `no-www-results.json` records both corrected passes.
The first fixture draft also referenced a nonexistent `LargeLocal.size` field;
its storage boundary was corrected before collecting behavioral baselines.
These were test harness failures, not production regressions.

Evidence is private under
`~/Library/Application Support/CoD2-native-decomp/F2-command-scheduling.noindex/`.
No game, VM, real network/download, linked integration build, performance
measurement or Linux x86 artifact comparison ran in this unit. Synthetic
message encoding does not prove packet-wire parity. Full gates, live acceptance,
reviews and publication remain with the coordinating task.

## Integration review

All five simplification rubrics completed with no edits. The focused correctness
and independent Claude review completed with no actionable findings; receipt
`/tmp/compound-engineering-501/ce-code-review/20261026-082502-8840b0a2/`.
The canonical stock/CoD2x CI registration received supplemental local review.

The review identified an unsupported assumption in the native non-1.3 fallback.
Root therefore limited all scheduling changes to patch 1.3, preserving the old
behavior for other native patches. Exact source projections against `f494bf9`
pass for all three files with native code disabled and with native non-1.3
selected, with WWW both enabled and disabled (`review-config-preservation.log`). This removes the new compatibility
risk without claiming Mac 1.0 reconciliation.

Failed detached WWW startup can leave the detached flag set with no active
transfer. Actual Mac assembly has that behavior too, so the proposed extra
cleanup was rejected as a fidelity change. Separately, native detached-download
completion still relies on connection-owned restart/list fields that disconnect
clears; the Mac keeps those fields static. That pre-existing mismatch and stale
connection-name mirrors after WWW completion remain unreconciled. The current
fixture proves preservation of an in-progress detached transfer, not its whole
disconnect/completion/reconnect chain.

Canonical integration passes all nine sanitizer groups in both builds and full
stock/CoD2x client and dedicated Release builds. Logs are `review-stock.log`,
`integration-codx.log` and `integration-build-{stock,codx}.log` under the evidence
root. No live download or packet-wire parity claim follows from these checks.
