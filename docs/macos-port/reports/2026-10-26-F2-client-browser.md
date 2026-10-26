# F2 — client browser packet handlers

Date: 2026-10-26. Base: `c2360e9`. Scope: `CL_ServerInfoPacket`,
`CL_ServerStatusResponse`, and `CL_ServersResponsePacket` in
`src/PC/client_mp/cl_main_pc_mp.c`.

Native patch 1.3 now follows the selected browser source when receiving an
unsolicited server announcement, clears the original fields when reusing a LAN
slot, and prints announcement/status text with the original delimiters. Stock
and CoD2x receive these corrections. The CoD2x protocol 118/120 extension and
explicit `debug_protocol` override retain their existing behavior.

## Mac comparison and changes

The complete three Mac 1.3 bodies were read as assembly. All 820 instructions
(3,586 bytes) in the private export were compared with the owned Mach-O bytes.
Original STABS establishes the field identities; no reference code or assembly
is included in this change.

- `CL_ServerInfoPacket`, `0x160f94`: matching pending pings still take priority,
  store `time - start + 1`, add nettype to the ping's info copy, and update known
  browser rows using the original received info. With no matching ping,
  `0x16122c–0x161239` reads `cls + 0x2b46c8`, **pingUpdateSource**, and returns
  when it is nonzero. The reconstruction read **waitglobalserverresponse**.
  Consequently internet/favorite browsing could admit unsolicited LAN rows
  after a master reply, while an outstanding master request suppressed LAN
  announcements even when LAN was selected.
- The LAN initialization stores at `0x16136d` and `0x1613a5` clear
  `maxClients` and `consoleDisabled`. Both were missing. Other fields retain
  exactly their previous reset/preservation behavior, including dirty=0,
  ping=-1, and preserving password, anonymous, and PunkBuster bytes.
- At `0x1613e7–0x1613fd`, the complemented string scan count includes the NUL;
  the `-2` indexed load therefore checks `info[strlen(info)-1]`. The C source
  incorrectly checked `len-2`. A one-character announcement read before the
  buffer, and a newline-terminated message acquired another newline. Native
  storage also reserves one additional byte for the original append after a
  maximum 1,023-character message; the original 1,024-byte local would overrun
  on that input. Copy capacity and printed text remain unchanged. This is a
  bounded memory-safety adaptation, not a claim to reproduce an original
  out-of-bounds write.
- `CL_ServerStatusResponse`, `0x161918`: `0x161bdc–0x161be1` stops before the
  next key/value field when no text remains. The reconstruction printed an
  extra blank value for an incomplete final key. At `0x161b94–0x161ba2`, the
  1,023-byte field limit is checked **before** advancing the input pointer.
  The native loop now preserves that order, including the original quirk that
  the last character of a full-length key becomes the following value. Normal
  status strings, player rows, completion timestamps, pending/retrieved flags,
  and unmatched-address behavior need no production changes.
- `CL_ServersResponsePacket`, `0x161f64`: endpoint storage, the 256-entry packet
  cap, the 20,000-row global cap, lookup/reset of previously known addresses,
  appending new rows and final sorting match the original flow. No code was
  changed here. The previously documented native parser from `c2d8e86` keeps
  pointer-width comparisons and bounded reads. It accepts a complete endpoint
  without requiring the following backslash, checks EOT before consuming an
  endpoint, and omits the original per-endpoint debug print. The Mac checks the
  following backslash before counting an endpoint and EOT after counting the
  previous one. These are retained native parser differences; this report
  does not claim malformed-packet equivalence. Within-packet duplicate entries
  are not newly deduplicated, matching the original use of the pre-packet count.

All new behavior is gated by `COD2_X64 && COD2_IS_PATCH_13`. There are no layout,
wire-format, server-discovery service, UI filter or network configuration changes.

## Verification

`tests/online/browser_packets.c` includes the complete production client module
and links the complete production `msg_mp.c`, `q_shared.c`, and CoD2x protocol
helper. Inputs are byte buffers read by the real message readers, then parsed by
the real info-string helpers. Console output, clock, dvar lookup, thread-local
format storage and address comparison are fixture services. No network call or
game process is required.

The unchanged fixture has ten groups: LAN source selection, stale-slot fields,
terminated/unterminated announcement lines, one-character announcements, maximum
announcement length, pending ping updates, protocol filtering, normal status
responses, incomplete status keys and maximum-length status keys. Before any
production edit, both stock and CoD2x failed seven groups; ping, protocol and
ordinary status controls passed. The one-character and maximum announcement
cases produced sanitizer failures. Both final builds pass all ten groups under
AddressSanitizer and UndefinedBehaviorSanitizer with assertions enabled.

The existing `master_response` fixture also passes unchanged in both modes,
including a packet crossing a 32-bit address boundary, a repeated packet and
an incomplete endpoint. Full production Release translation-unit compilation
passes in both modes. These are local packet-handler checks; no live browser,
external master query, linked game smoke, or original runtime replay was run.
The parent integration owns those broader gates.

Commands, run under `taskpolicy -b nice -n 19` for compilation:

```sh
python3 tests/online/run_browser_packets.py --compile-db <engine-db> --output <private-output>
# The same fixture can load the historical client source:
python3 tests/online/run_browser_packets.py --compile-db <engine-db> --output <private-output> --baseline c2360e9
python3 tests/online/run.py <engine-db> master_response
python3 tools/abi/legacy.py --base c2360e9
python3 tests/fixes13/legacy.py --base c2360e9
git diff --check
```

Exact `unifdef` output followed by `cmp` matches the base for both
`-UCOD2_X64` and `-DCOD2_X64=1 -DCOD2_IS_PATCH_13=0`. The ABI legacy checker
reports one file, five configurations and zero mismatches; the fixes13 checker
reports ten configurations and zero mismatches. These source-preservation checks
do not prove Linux i386 object identity. Existing warnings in the complete
translation units remain; no new diagnostic appears in the changed logic.

Private logs, regression failures, compiled fixtures and byte-comparison counts
are in `~/Library/Application Support/CoD2-native-decomp/F2-client-browser.noindex/`.
This unit does not reconcile the rest of `cl_main_pc_mp.c` or close the full F2
plan.

## Integration review

Canonical stock and CoD2x runs also pass all ten groups; both Release clients
and dedicated servers link. The packet-handler runner is registered in CI for
both modes. Five independent simplification lenses found no code changes; one
redundant allocation comment was removed. Proof tests remain unchanged.
The focused review, including an independent Claude Opus 5.5 pass, completed
with zero actionable findings. Receipt:
`/tmp/compound-engineering-501/ce-code-review/20261026-115413-1776b511/`.
Requested high effort is not independently verified. The fixture directly sets
the browser source, so the globalservers-to-localservers command transition and
consecutive/empty status delimiters remain untested. No live external master
query was performed.

## Final integration gates

The combined integration source passes both full function ABI audits (634 stock
and 651 CoD2x translation units), callback, strict-AST and import checks. All four
Release targets link. Every changed engine file preserves exact
`unifdef -UCOD2_X64` output against `c2360e9`; the five-configuration guard
comparison and ten-configuration fixes13 comparison also pass. The general
`tools/abi/legacy.py` invocation reports ten mismatches solely in the two
intentionally changed online test-harness files, not engine source. This result
is retained rather than labeled a fully green invocation. Evidence:
`F2-wave13.noindex/final-gate-results.json` and corresponding `final-*.log` files.
