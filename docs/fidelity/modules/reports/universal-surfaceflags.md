# universal/surfaceflags module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 2 equal.** No code correction was needed.

Compared complete `Com_SurfaceTypeFromName` at Mac 0xbcad0: case-insensitive default handling, the 22-name search, surface-bit extraction and -1 miss. Compared `Com_SurfaceTypeToName` at 0xbcb46: index-minus-one range check, indexed return and default fallback.

A private script compared all 54 native `infoParms` names and all four integer fields with the Mac table at 0x348820. All rows match, including both zero terminators. This confirms the existing typed native table rather than reintroducing address-shaped legacy initializers.

The arm64 Release dedicated-server build passed at low priority with two jobs. Functions were hand-compared and data was checked directly; no runtime surface-name test, full CI or replay was run by this worker. No new tests were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-text.noindex/`.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
