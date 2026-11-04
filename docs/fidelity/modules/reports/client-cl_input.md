# client/cl_input module reconciliation

Updated 2026-11-04.

**85/85 reconciled: 78 equal, 3 fixed, 4 intentional.** Mac 1.3 i386 is the authority. All assigned functions and their active inline helpers were compared. No Windows-only input difference was identified that required changing the native platform boundary. The OFF source text is preserved.

## Corrections

**Held keys with no timestamp accumulated the wrong time.** The `CL_KeyState` helper added `com_frameTime` to previously accumulated milliseconds. Mac replaces those milliseconds, for example at 0x189894 in `CL_AdjustAngles` and 0x189a2e in `CL_KeyMove`. Both callers now use the replacement behavior. The rest of their timing, stance and movement arithmetic matches.

**Mouse speed limits stopped after the first clamp.** Mac applies the upper bound and then the lower bound, at 0x18922d–0x189267 for yaw and 0x1892f3–0x18932d for pitch. Returning after the upper bound differed when a maximum speed was negative. Native now applies both bounds in order. Existing finite-value fallbacks, CoD2x mounted-gun sensitivity, and gamepad ownership notifications remain in place.

## Other comparisons and native boundaries

All 69 input command registrations and all 69 removals match the Mac names, handlers and order. The three stock dvar registrations match their values, ranges and flags. The optional gamepad initialization/shutdown and CoD2x `sensitivity_MG` registration remain deliberate additions.

The packet and voice-packet functions preserve the compared headers, loops, command masks, checksum inputs, compression, packet history and transmission behavior. Their native stack buffers replace the Mac `LargeLocal` allocation/unwind backend. The packet debug hook is a no-op. Absolute mouse events remain a native platform addition alongside the matching relative event path.

The review also covered every button wrapper, two-key edge tracking, stance overrides and tap/hold behavior, command creation, angle conversion and send pacing. For example, `IN_GoStandDown` preserves the Mac ordering that can press jump before the held-stance gate; `CL_CmdButtons` uses the Mac's unsigned movement-type range 2 through 4.

## Verification and limits

The arm64 Release dedicated target built successfully at low priority with two jobs. The dedicated graph omits client code, so both owned client sources were also compiled directly with the actual client compile flags; both compiled without diagnostics. The existing stance fixture passed nine groups, including command controls, button edges and stance timing. The existing UI key-routing fixture also passed. No tests were added or changed, and no full CI or replay was run.

The zero-downtime accumulated-time case and the negative mouse speed-limit case were compiled and reviewed but not exercised individually. Packet and voice transmission were not exercised by this worker. Five OFF preprocessing configurations match the base, and neither changed source uses `__LINE__`.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-input.noindex/`. The complete per-entry verdicts are in the private `client-input.ledger.json` unit ledger.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
