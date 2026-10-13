# WS83 — Defined native server challenge arithmetic

2026-10-07, composed base `7c9a2a2`. WS76's broader connection fixtures exposed
undefined behavior in `SV_GetChallenge`: `rand() << 16` shifts a signed integer
beyond its representable range. Its bounded identity fixture retained a small
deterministic random stream and recorded the limitation.

The Mac 1.3 binary at `SV_GetChallenge` (`0x17daae`) calls the random generator
twice at `0x17dee7` and `0x17deee`, shifts the first 32-bit result at `0x17def3`,
then XORs the second result and server time at `0x17def6`/`0x17defe` before
storing the 32-bit challenge. Native code now performs this calculation using
`uint32_t`, with the two calls explicitly sequenced, then copies the resulting
bits into the signed field. Stock and CoD2x native builds use the correction;
the inactive native branch retains the original source. This changes neither
the random source nor challenge/admission policy, packet format or struct layout.

`tests/cod2x/challenge_random.c` drives the actual extracted `SV_GetChallenge`
with six fixed pairs and literal expected bit patterns. Cases include zero,
the signed-shift boundary, `INT_MAX`, mixed high/low values and negative server
time. Both original stock targets fail UBSan on the `32768 << 16` case before
the correction. The existing host protocol runner now executes these vectors
and treats sanitizer findings as failures with `-fno-sanitize-recover=all`.

All six vectors and existing admission/auth/discovery cases pass on stock and
CoD2x client/dedicated configurations with ASan/UBSan. All four production
translation-unit configurations compile with actual Release flags; scoped
ABI audits report two TUs per mode and zero errors/mismatches/new findings.
Five native-off preprocessing configurations and two actual SDK commands with
the native macro removed match the base exactly. `git diff --check` passes.

Private evidence: `CoD2-native/ws83-random-red`, `ws83-stock`, `ws83-codx` and
`ws83-compile` under Application Support. The independent binary instruction
receipt remains private. This is deterministic arithmetic/production-path
evidence; it does not certify live authorization, randomness quality or public
server admission. Parser and compiler findings remain WS80's separate work.
