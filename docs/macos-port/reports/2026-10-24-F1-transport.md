# F1 transport: transmission limit and crash localization

Baseline: `6c6edd0343259966e2697c8ca561fe6d1c2c5cbe`, verified before edits.
No staging, commits, build-directory writes or full builds performed.

`behavior_changed: true`: native `Netchan_Transmit` now accepts messages through
`MAX_MSGLEN` (128 KiB), instead of rejecting everything above 16 KiB. The
native buffers already hold 128 KiB. Legacy source remains unchanged after
`unifdef -UCOD2_X64` (byte comparison against baseline passed).

Mac authority: `Netchan_Transmit` (`__Z16Netchan_TransmitP9netchan_tiPKh`)
compares length to `0x20000` at `0x159036`, with signed greater-than branch to
`Com_Error`; `0x15904c` selects fragmentation above 1299. Independently written
native change uses the established native message-size constant. No copied
reference source, disassembly or proprietary fixtures are stored here.

Bounded classification: transmit limit **fixed**; server transmit fragment
header, 1300-byte splitting, zero-size terminator on exact multiples, and
sequence advancement **equal** for tested cases. Mac fragment instructions at
`0x158df6–0x158e33` select the remaining length and write the server's long
offset/short length; `0x158ebc–0x158f09` retain the sequence until the final
short fragment. `NET_SendPacket`'s native platform-send wrapper remains an
**intentional platform replacement** (void platform send returns success).
Other transport/profile/receive functions have not been fully reconciled;
this report does not claim file-wide fidelity.

The reported SIGBUS is **not fixed by this size correction**. The crash log's
PC `0x1029984b0`, executable slide `0x102938000`, maps to
`NET_SendPacket` at unslid `0x1000604b0`. In the matching existing dedicated
binary, this loads the enabled byte at offset 16 from the cached `showpackets`
dvar pointer. Register x8 is `0x6053d0300`; its +16 equals the reported fault
`0x6053d0310`. `Netchan_Transmit+0x18c` in the backtrace is the return address
from that call. Thus this is an invalid dvar pointer at packet send, not an
observed fragment-copy crash. Pointer corruption/lifetime investigation is
outside these two transport files and was handed directly to the dvar worker
and root. No speculative null/bounds guard masks the fault.

Inspected existing tests: `tests/lp64/game/netchan.c` covers server profiles
and XOR with a stubbed transmitter; `tests/online/netchan_decode.c` covers the
client challenge key; `tests/fuzz/targets/netchan.c` exercises receive/reassembly.
None tests the actual transmitter's maximum length.

New `tests/lp64/network/f1_transport.py` compiles one fixture sequentially using
actual engine flags with assertions and ASan/UBSan. It includes the production
translation unit and supplies deterministic message serialization/platform-send
stubs. It proves the transmitter's actual state and packet bytes, not real
socket I/O or receive roundtrips. Cases: lengths 0, 1299, 1300, 2600, 16385,
131072, and rejection at 131073; payload preservation, server fragment header,
sequence advancement and exact-multiple terminator. Profiling disabled.

Exact red command (before production edit; exit 250, unexpected `Com_Error` at
16385 asserted in fixture line 22):

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/network/f1_transport.py --compile-db build-macos/compile_commands.engine.json --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-transport.noindex/red"
```

Exact green command (exit 0; `f1_transport: Mac 128 KiB cap and fragment bytes pass`):

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/network/f1_transport.py --compile-db build-macos/compile_commands.engine.json --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-transport.noindex/green"
```

Evidence directories contain compile/run logs and exact command. Compile lane
released to root. Root verification: rerun the green command with a fresh output
directory; rebuild stock/CoD2x targets and reproduce the prior invalid-weapon
scenario after the dvar worker's correction. Full builds, live crash acceptance,
CoD2x fixture, client fragment encoding, profiler-enabled transport, actual
message serializer, and Linux i386 byte comparison were not run by this worker.
`sv_net_chan_mp.c` was read but remains unmodified.
