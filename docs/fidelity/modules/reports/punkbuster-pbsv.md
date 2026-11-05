# punkbuster/pbsv reconciliation

Updated 2026-11-05.

38/38 reconciled: 37 existing intentional rows retained exactly, 1 fixed engine-helper row. The prior bundled PunkBuster verdicts are unchanged: they make no semantic-equivalence claim, and native uses the documented null adapter in src/PC/punkbuster/pb_null_adapter.c rather than loading the discarded upstream backend.

The extra itoa entry at Mac 0x25994 is an i5stdlib engine helper, not part of that exemption. Mac negates every negative input regardless of radix, extracts signed remainders/quotients, emits uppercase digits and an optional minus, then reverses strlen bytes. Native previously applied the minus only in radix 10 and treated other negatives as unsigned. It also evaluated signed -INT_MIN. Native now follows the all-radix sign rule, uses explicit 32-bit wrapped negation, and retains the Mac signed-remainder behavior for INT_MIN. Output is written directly before reversing, preserving the original C-string reversal contract.

The arm64 Release dedicated target compiled and linked this source. The correction was reviewed against 0x25994-0x25a58 but not exercised. OFF preprocessing matches the base and there are no __LINE__ expansions. No new tests, full CI or replay ran. No bundled library was reconstructed.
