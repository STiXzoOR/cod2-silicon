# WS97 — Preserve admitted identity through NAT port translation

2026-10-09. Branch `codex/nat-identity`, base
`048ff5e3004807e41a3f263179bc5d62efc717f2`. Read `CONTRIBUTING.md` and
`docs/macos-port/PLAN.md` before changes. The candidate is a **real native
CoD2x regression**, reproduced before the production fix. No stock packet
behavior or wire format changes.

## Reference and root cause

The private Mac 1.3 binary establishes the connection-routing contract:

- `SV_PacketEvent` (`__Z14SV_PacketEvent8netadr_tP5msg_t`) calls
  `NET_CompareBaseAdr` at `0x15b93f`, checks qport at `0x15b94b`, and stores the
  translated port at `0x15ba25`, before `Netchan_Process` at `0x15ba3c`.
- `SV_VoicePacket` (`__Z14SV_VoicePacket8netadr_tP5msg_t`) compares the base
  address at `0x159f02`, qport at `0x159f0e`, and stores the new port at
  `0x159f4a`. Both match the unchanged native production handlers.

Inspected with `nm -a` and `otool -arch i386 -tV -p <symbol>` against
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`. Local, bounded symbol
extracts are in the evidence directory; no disassembly is committed.

CoD2x reference `d8c54695a5239ac99d1dfe96212b809b1a54bfee` (1.4.6.8):
`src/shared/gsc_player.cpp:65–104` reads `clientPBguid` and `PBguid` directly
from `svs_clients[entnum]`, with no address/port membership check.
`cod2_server.h:118–119` declares these per-client strings;
`server.cpp:839–841` records the supplied hash in the challenge, and
`server.cpp:684–686` records the authorization reason in its `PBguid`.
The admission wrapper in `server.cpp:376–487` delegates to the original
admission after its extension checks. These accessors do **not** share our
sidecar's translated-port limitation. Fixing native identity retention
therefore restores the observable CoD2x contract, rather than choosing a new
behavior over a shared reference defect. Reference code was inspected, not
copied into the implementation.

At `048ff5e`, `SV_Cod2xClientIdentity` requires the client slot's current
full address to equal its admission address. `NET_CompareAdr` checks UDP
ports for `NA_IP`. The Mac-compatible game and voice handlers update that
port, so the getter rejects the same admitted client. Consumers are
`SV_Cod2xClientHwid`, `SV_Cod2xClientDigest`, `SV_Cod2xClientAuthorization`
(the three script accessors), and `SV_Cod2xStoreAuthorization`'s late-reply
loop. `getIp` reads the current client address separately and is unaffected.

Admission stores the sidecar before `ClientConnect`. Reconnect/slot setup,
mod rejection and `SV_FreeClient` clear it; `SV_FreeClients` and
`SV_Shutdown` clear all identities. `SV_ChangeMaxClients` moves client storage,
so retaining slot provenance rather than an allocation pointer remains
necessary. Those consumers/lifecycle boundaries are unchanged.

## Change and fixture

Only `src/PC/server_mp/sv_client_mp.c` changes in production, inside the
existing `COD2_X64 && COD2_CODX` gate. The private sidecar captures the admitted
qport. Membership now requires an occupied slot, the original challenge,
that qport, and the same base address for `NA_IP`; other address types retain
`NET_CompareAdr` behavior. Qport distinguishes connections behind the same IP
and prevents a changed channel from inheriting the old identity.

The full **admission** address remains stored. Challenge metadata and late
reply correlation still require the original exact address/port, challenge
and supplied token. Updating the stored address to the translated port would
break this correlation, so the fix changes membership instead. No packet
handler, shared client/challenge layout, network serializer, port-rewrite
order, host advertisement or host revision changes. `COD2X_HOST_REVISION`
remains **0**.

`tests/lp64/script/identity_methods.py --nat-only` adds a focused mode to the
existing identity harness; cases live in `tests/lp64/game/nat_identity.c`.
The runner extracts production admission, metadata, authorization,
`NET_CompareAdr`, `NET_CompareBaseAdr`, `Netchan_Setup`, `Netchan_Process`,
`SV_PacketEvent`, `SV_VoicePacket`, method resolution, compiler emission and
VM builtin dispatch bodies using actual engine flags/headers. No copied
routing or identity algorithm is under test. Services for game/voice
consumption, decoding and profiling are seams. Packed message reads use
fixture `memcpy` readers; this fixture does not certify production message
reader alignment, decoding, payload execution, sockets or real internet
traffic. Identity values are read through the real script resolver,
compiler/VM call and string-return boundary.

Cases exercise game and voice translation independently, real late replies,
timeout admission then translation then a late reply, old game sequence
rejection after port rewrite, different IP/same qport, same IP/unknown qport,
two admitted clients behind the same IP with different qports, changed
challenge/base IP/qport membership, actual free and slot reuse, a stale old
reply, and a forged authorization source. CoD2x runs both production revision
0 (hash/reason) and synthetic revision 6 (also admitted HWID). Stock runs the
same packet/admission scenarios and proves extension methods remain absent.
All addresses, identities and authorization reasons are synthetic.

## Verification

Evidence: `~/Library/Application Support/CoD2-native-current-readiness/WS97.noindex/`.
All acceptance compiles/checks ran under `taskpolicy -b nice -n 19`, with at
most two concurrent compiler jobs. `verification-receipt.json` records exact
commands, exit codes and durations for the 22 primary checks.
A supplementary historical-difference attribution initially ran without the
priority wrapper; its final confirmation was repeated under the required
policy (`inactive-attribution-policy.log`). This did not run an engine/game.

- **Red before fix:** `--nat-only --baseline 048ff5e` with the CoD2x engine
  database fails **107 assertions per target** (listen and dedicated), exit 1:
  `red-codx-2.log`. Failures are identity value assertions, not setup errors.
  Stock baseline controls pass both targets, `red-stock.log`. The earlier
  `red-codx.log` is a fixture setup failure (missing declarations/type),
  retained honestly and excluded from behavioral red evidence.
- **Green:** focused NAT mode passes stock and CoD2x, listen and dedicated,
  zero failures: `nat-{stock,codx}.log`. Both existing full identity suites
  pass with `--parser-ubsan`, including their real parser tests.
- Existing host protocol (including challenge-random vectors), host lifecycle
  in listen and dedicated modes, and UDP operations fixtures all pass in
  stock and CoD2x. `sh tests/cod2x/run.sh` also passes.
- All **four** full changed-TU combinations compile with unchanged Release
  flags (stock/CoD2x × listen/dedicated). Focused function ABI audits cover
  both entries per mode: **zero errors and zero mismatches**.
- Delta checks against `048ff5e`: legacy ABI source guards, **2 files × 5**
  native-OFF configurations, zero mismatches; CoD2x inactive guards,
  **2 files × 17** configurations, zero mismatches; fixes13 guards,
  **1 engine file × 10** configurations, zero mismatches.
- `unifdef -UCOD2_X64` on the base and changed engine file followed by `cmp`
  returns **0**. Both filtered files have SHA-256
  `e8f22e7ceef1c85e66ca636100c9a41344dc6dbc841fb6eb8fb984dfe704d442`;
  `native-off-receipt.json` records commands. `unifdef` itself returns 1
  because it removed gated text, its normal successful-change status.
- Release-lineage `tools/abi/legacy.py --base 664f1a1` remains **red**, exit 1:
  **91 files × 5 configurations, 10 differences**. Five are generated
  `build/lp64_gen/data_native.c` (excluded by CMake OFF); five are the test-only
  `tests/platform/server_control.c`. Receipt: `legacy-release-lineage.log`.
- Release-lineage `tests/cod2x/check_inactive_gates.py --base 664f1a1` remains
  **red**, exit 1: **92 files × 17 configurations, 457 differences**.
  The brief's **436** is the earlier frozen-head count. The extra **21** are
  already present at starting head `048ff5e`: seven each in
  `cg_scoreboard_mp.c`, `r_bsp.c` and `speex/filters.c`, reflecting intentional
  stock/native repairs plus synthetic x64-defined-zero configurations.
  The same production guard comparator applied to historical source versions
  confirms this attribution (`inactive-additional-differences-policy.json`);
  the full starting-head comparison is `inactive-starting-head.log`.
  This unit's zero-difference delta check is separate; no gate or allowlist
  was weakened. Receipt: `inactive-release-lineage.log`.

Reproduce configuration and the principal checks from this worktree (use
fresh output paths; fixture runners reject existing output directories):

```sh
EVIDENCE="$HOME/Library/Application Support/CoD2-native-current-readiness/WS97.noindex"
taskpolicy -b nice -n 19 cmake -S . -B build-ws97-stock \
  -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake -S . -B build-ws97-codx \
  -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
for mode in stock codx; do
  db="build-ws97-$mode/compile_commands.json"
  taskpolicy -b nice -n 19 python3 tests/lp64/script/identity_methods.py \
    --nat-only --compile-db "$db" --output "$EVIDENCE/reproduce-nat-$mode"
  taskpolicy -b nice -n 19 python3 tests/lp64/script/identity_methods.py \
    --parser-ubsan --compile-db "$db" --output "$EVIDENCE/reproduce-identity-$mode"
  for fixture in run_host_protocol host_lifecycle udp_operations; do
    taskpolicy -b nice -n 19 python3 "tests/cod2x/$fixture.py" \
      --compile-db "$db" --output "$EVIDENCE/reproduce-$fixture-$mode"
  done
  taskpolicy -b nice -n 19 python3 tests/cod2x/host_lifecycle.py --dedicated \
    --compile-db "$db" --output "$EVIDENCE/reproduce-lifecycle-dedicated-$mode"
  taskpolicy -b nice -n 19 python3 tools/abi/audit.py \
    "$EVIDENCE/$mode-client-compile-db.json" --jobs 2 \
    --baseline tools/abi/baseline.json --output "$EVIDENCE/reproduce-abi-$mode.json"
done
# Expected red: 107 failures in each CoD2x target.
taskpolicy -b nice -n 19 python3 tests/lp64/script/identity_methods.py \
  --nat-only --baseline 048ff5e --compile-db build-ws97-codx/compile_commands.json \
  --output "$EVIDENCE/reproduce-red-codx"
taskpolicy -b nice -n 19 sh tests/cod2x/run.sh
taskpolicy -b nice -n 19 python3 tools/abi/legacy.py --base 048ff5e
taskpolicy -b nice -n 19 python3 tests/cod2x/check_inactive_gates.py --base 048ff5e
taskpolicy -b nice -n 19 python3 tests/fixes13/legacy.py --base 048ff5e
# These remain red; keep their counts separate from the delta checks.
taskpolicy -b nice -n 19 python3 tools/abi/legacy.py --base 664f1a1
taskpolicy -b nice -n 19 python3 tests/cod2x/check_inactive_gates.py --base 664f1a1
git show 048ff5e:src/PC/server_mp/sv_client_mp.c > "$EVIDENCE/off-base.c"
unifdef -UCOD2_X64 "$EVIDENCE/off-base.c" > "$EVIDENCE/off-base-filtered.c"
unifdef -UCOD2_X64 src/PC/server_mp/sv_client_mp.c > "$EVIDENCE/off-current-filtered.c"
cmp "$EVIDENCE/off-base-filtered.c" "$EVIDENCE/off-current-filtered.c"
git diff --check
```

## Limits and composition

The existing authorization reply carries a challenge but no digest/request
generation. The disclosed same-challenge digest-retry correlation limitation
is retained; the script reason is raw reply state, not proof of independent
validation of the current supplied hash. The fix does not strengthen packet
authentication; it preserves identity across the routing behavior the Mac
already uses.

No game/GUI, dedicated live server, live authorization, gameplay timing,
rendering/audio check or benchmark ran. No bench lease or QA app was needed.
A full engine link, the complete composed merge gate and an i386 artifact
comparison are not claimed. The Linux multilib comparison remains unavailable
on this Mac. Release-lineage legacy ABI and CoD2x inactive checks remain
known red; their receipts are recorded separately from this unit's green
delta guards below.

Composition is limited to this one native-gated engine TU and the identity
fixture extension/report. No other worktree, remotes, existing commits,
licensed data, real player data folder or host readiness constant was changed.
All work is committed. Scratch CMake directories in this worktree were
removed after retaining their compile databases and receipts in evidence.
No scratch CMake build directories were created outside this worktree.
