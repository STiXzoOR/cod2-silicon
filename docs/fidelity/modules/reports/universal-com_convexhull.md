# universal/com_convexhull module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 2 equal.** `Com_ConvexHull` at 0xfb1ee and `Com_RecursivelyGrowHull` at 0xfafa0 were compared by hand with Mac 1.3 symbol-start disassembly. Constant and call/string triage checks also ran.

The extrema tie choices, normalized edge equations, +/-0.001 thresholds, two-ended partition scans, swaps, front/back index adjustments, insertion memmoves and recursive call order match. The original degenerate-input behavior is preserved, including the zero result for the exhausted initial partition. No code was changed.

The arm64 Release dedicated-server target builds at low priority with two jobs. No new tests, full CI or replays were run. Equality here is from code comparison, not runtime execution.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-math.noindex/`.
