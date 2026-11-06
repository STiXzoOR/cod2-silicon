# F2 detached download lifecycle

2026-10-26; base `352d494`; native patch 1.3, stock and CoD2x.

Detached WWW downloads now retain the remaining file queue and filesystem-restart
request across `CL_Disconnect`. Previously those two values lived in the native
connection object, which disconnect zeroes: successful HTTP completion could
enter map loading without restarting the filesystem, and lose the remaining local
queue. The existing `clientStatic_t` fields now own both values throughout
initialization, next-file selection and completion. No shared layout changed.

Initialization clears old static transfer state using the existing helper.
Disconnect retains the displayed download name while detached. Successful WWW
completion clears the native connection-name mirrors as well as the static
names before selecting another file. UDP still uses its existing connection
handle and names; its completion reaches the same static queue/restart path.
All changes are limited to native patch 1.3. Native non-1.3 and `COD2_X64=OFF`
source projections are unchanged.

## Reference evidence

Mac MP 1.3 assembly is the authority. Nine relevant production bodies were read;
930 exported instruction byte sequences match the owned binary, with zero
mismatches. Pseudocode and external source were not copied into the changes.

- `CL_InitDownloads` (`0x14d84c`) clears restart, names and WWW state at
  `0x14d86a–0x14d8a7`, then passes the static list at `0x15ca270` to
  `FS_CompareIwds` (`0x14d916`).
- `CL_NextDownload` (`0x14d784`) reads that static list, calls the real begin
  function, sets static restart at `0x15ca258` (`0x14d7f7`), then moves the
  remaining list into the same static buffer (`0x14d817`).
- `CL_Disconnect` (`0x14b8ea`) skips ordinary download/UI cleanup when detached
  (`0x14b913–0x14b93e`), clears the connection object (`0x14b9ca–0x14b9e2`),
  and preserves static state when detached (`0x14b9f2–0x14b9fa`).
- `CL_DownloadsComplete` (`0x14d368`) reads static restart (`0x14d373`),
  restarts the filesystem, suppresses `donedl` while detached, then clears
  static transfer state (`0x14d534–0x14d5e3`).
- `CL_WWWDownload` (`0x1625cc`) clears names after installation, queues
  `reconnect` when detached, clears in-progress and calls `CL_NextDownload`
  (`0x16275d–0x1627c4`). Queuing reconnect does not immediately reconnect;
  remaining-file selection still occurs first. The native connection-name
  cleanup preserves its UDP mirror of these original static fields.

The unchanged `CL_ParseWWWDownload` failure branch sets the detached flag even
when downloader startup failed (`0x1d598d–0x1d59c5`, branching back to
`0x1d596c–0x1d5978`). That original quirk remains and has a passing control.
The clear helper also leaves the static queue itself untouched, as the original
assembly does; successful list production overwrites it.

## Verification

The existing scheduling fixture now includes the complete production
`CL_InitDownloads` body. Three added groups cover stale-state initialization
followed by real UDP completion; successful detached HTTP completion and local next-file handoff; and the original failed detached-start behavior. The local sequences run real initialization, begin, WWW redirect, disconnect,
WWW completion, next-file selection and filesystem-restart decisions. They check preserved UI
state, installed destinations, reconnect queuing, remaining-file reliable
commands, cleanup and suppression of `donedl` while detached. Existing scheduler,
packet-history, UDP, normal WWW and disconnect checks remain; the connected WWW
case additionally checks connection-name cleanup.

The prior helper that begins isolated scheduling tests seeds both possible list
locations, allowing the unchanged baseline and repaired implementation to start
from the same queue. The new lifecycle groups instead obtain their list from
`CL_InitDownloads` through the synthetic filesystem boundary.

The runner uses actual engine flags, enables assertions and ASan/UBSan, and
compiles all three selected client translation units. Actual network, downloader,
filesystem and command-buffer execution are replaced with synthetic boundaries.
No files are downloaded or installed. Reconnect is observed as a queued command;
a fresh server connection and packet-wire parity are not claimed. In the
two-file local handoff case the second redirect is injected while disconnected,
without executing reconnect or a fresh gamestate. It tests retained local state,
not a complete two-file network lifecycle.

Both stock and CoD2x pass all 12 groups with default settings, with
`--net-hardening`, and with `--no-www`. The no-WWW variant runs initialization
and UDP/disconnect controls using the real disabled downloader helpers, and
skips HTTP execution. Both unchanged `--baseline 352d494` runs fail three groups
with 11 intended assertions: stale connected names, stale initialization state,
and the detached completion chain. All other groups, including the failed-start
quirk, pass on that baseline.

```sh
# Run serially for build-macos and build-macos-codx, with unique private outputs.
taskpolicy -b nice -n 19 python3 tests/input/run_scheduling.py \
  --compile-db <integration-build>/compile_commands.engine.json \
  --output <private-output>
# Repeat with --baseline 352d494, --net-hardening and --no-www.
python3 tests/fixes13/legacy.py --base 352d494
```

The ten fixes13 legacy configurations pass. Direct `unifdef` comparisons against
`352d494` are byte-identical for both changed engine files with native code
removed, and with native non-1.3 selected with WWW enabled and disabled. These
are source-preservation checks, not Linux i386 object comparisons.

Private evidence is under
`~/Library/Application Support/CoD2-native-decomp/F2-download-lifecycle.noindex/`.
`results.json` records the eight final commands and statuses; `stock-baseline`
and `codx-baseline` logs record intended red results. An earlier fixture draft
also counted initialization cancellation in its failed-start control; that
control now measures only the cancellation caused by failed startup. Its first
12-assertion baseline log remains separate from the final 11-assertion logs.

No live game, real network/download, VM, linked build, performance measurement,
or Linux x86 artifact comparison ran in this unit. Broader client-module
reconciliation, integration builds, review and publication remain with the
coordinating task.

## Review follow-up

The completed correctness/Claude review retained one P2: CoD2x demo
initialization bypasses ordinary static-download cleanup. A detached transfer,
including the original failed-start quirk, can retain `downloadRestart` across
Disconnect; the new static restart consumer then returns before demo cgame
initialization. The native patch-1.3 demo shortcut now clears transfer state
before completion. The ordinary Mac initialization ordering is unchanged.
Review finding 2 was a coverage qualification: the two-file test proves local
handoff only. The report now states that boundary explicitly.

The new production-function regression starts both a detached active transfer
and the preserved failed-start case, calls real Disconnect, establishes demo
connection state, then invokes real initialization and completion with synthetic
renderer/filesystem boundaries. Before the correction, its two cases fail four
assertions while all other groups pass: filesystem restart runs and cgame
initialization is not reached. Afterward all 13 groups pass in stock and CoD2x,
including hardened-network and disabled-WWW variants. The new demo group runs
its two cases only with CoD2x and WWW enabled; other variants retain their
applicable controls. This fixture does not parse a demo file or execute a real
HTTP transfer.

All stock and CoD2x client/dedicated Release targets link after the correction.
Fresh exact OFF/native-non-1.3 projections match `7e1e610` for both engine
files, and all ten fixes13 legacy configurations pass. The review's sole
P2 is resolved; its local-handoff coverage qualification remains explicit.
Private integration evidence includes `review-demo-red.log`,
`integration-results.json`, `integration-preservation.log` and
`integration-legacy.log`.

Fresh linked demo smoke runs also complete with the existing private stock and
CoD2x combat demos (684 and 685 frames respectively), then exit normally. These
exercise ordinary demo initialization after the change, not an actual detached
HTTP-to-demo transition. The first driver attempt placed the demo in unsearched
`home/main/demos` and exited with file-not-found; the corrected fixture uses the
private `home/raw/demos` search directory. Both attempts are retained. The VM
was suspended for native runs and restored to started; the game lease was
released. No performance conclusion is drawn from these capped smoke runs.
