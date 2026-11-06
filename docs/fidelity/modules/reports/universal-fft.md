# universal/fft module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 2 equal.** `FFT_Init` at 0xf9ff8 and `FFT` at 0xfa080 were compared by hand with Mac 1.3 symbol-start disassembly, supplemented by constant and call/string triage checks. Starting at the exact FFT_Init symbol avoids the misaligned prologue in full-binary otool output.

The 256 eight-bit reversals, exact double angular step, conversion to float, sin/cos calls and imag/real stores match. The transform's permutation, four-point butterfly write order, trig indices and later complex butterfly stages also match for the engine's FFT sizes and strides. The original minimum four-point storage contract was retained. No code was changed.

The arm64 Release dedicated-server target builds at low priority with two jobs. The existing water fixture was inspected. No tests were added or changed, and no full CI or replay was run; these functions were not exercised by this audit.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-math.noindex/`.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
