# gfx_d3d/r_cmds module reconciliation

Updated 2026-11-05.

**5/5 reconciled: 5 equal.** The Mac bodies at 0xf328c through 0xf3394 were read against native source, supplemented by triage and constants. No source correction was needed.

The three command wrappers pass the original picmip call and screenshot types 1/0. All nine registration strings and all ten removal strings were decoded from the original binary and checked in order, including the extra r_vc_stats removal. Callback pointer slots were resolved to their original symbols: image list, material reload, sun load/save, and static-model cache stats/flush match the native direct-function aliases. This verifies registration identity, not the behavior of every registered callback.

Verification: r_cmds.c compiled with the actual local arm64 Release command. The dedicated target built at low priority with two jobs. No new tests, full CI or replays were run; console registration and screenshot capture were not runtime exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-commands.noindex/`.
