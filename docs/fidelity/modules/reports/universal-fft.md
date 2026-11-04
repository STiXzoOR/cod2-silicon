# universal/fft module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 2 equal.** `FFT_Init` at 0xf9ff8 and `FFT` at 0xfa080 were compared by hand with Mac 1.3 symbol-start disassembly, supplemented by constant and call/string triage checks. Starting at the exact FFT_Init symbol avoids the misaligned prologue in full-binary otool output.

The 256 eight-bit reversals, exact double angular step, conversion to float, sin/cos calls and imag/real stores match. The transform's permutation, four-point butterfly write order, trig indices and later complex butterfly stages also match for the engine's FFT sizes and strides. The original minimum four-point storage contract was retained. No code was changed.

The arm64 Release dedicated-server target builds at low priority with two jobs. The existing water fixture was inspected. No tests were added or changed, and no full CI or replay was run; these functions were not exercised by this audit.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-math.noindex/`.
