# client/screen_placement_mp module reconciliation

Updated 2026-11-04.

**9/9 reconciled: 8 equal, 1 fixed.** Complete Mac 1.3 bodies and SSE arithmetic order were compared. Four alignment tables at 0x33bc40–0x33bca0 were decoded. Exact-symbol disassembly confirmed the four single-multiply converters and the empty split-screen text offset helper.

## Correction

`SetScreenScaling` selected the opposite safe-area bound for unordered float differences. Mac 0x18ebfa, 0x18ec19, 0x18ec3b and 0x18ec5d use `cmpnltss`, which accepts unordered values; native used `>= 0`. Native now uses `!(difference < 0)` for the same four selections. Finite aspect, safe-area and scale arithmetic already matched.

## Verification and limits

No new tests, full CI or replays were run. Existing input/platform probes were inspected; they do not cover unordered safe-area values. The arm64 Release dedicated target built, and the client translation unit compiled separately with the configured client flags. All five OFF preprocessing configurations match the base; this file has no `__LINE__` expansions. The correction is compiled and reviewed, not exercised. No rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-screen.noindex/`.
