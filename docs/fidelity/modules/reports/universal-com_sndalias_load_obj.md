# universal/com_sndalias_load_obj module reconciliation

Updated 2026-11-04.

**34/34 reconciled: 25 equal, 7 fixed, 2 intentional.** Mac 1.3 i386 is the authority. All exported implementations and Linux-only helpers were compared, with inline identities established in their Mac callers. Changes are under COD2_X64; the legacy preprocessed source is unchanged.

## Corrections

Permanent aliases now start `iSequence` at zero, as Mac 0xb6e50 does. Native had copied the CSV sequence, which is used to order build records before playback.

Subtitle lookup now resumes the outer reference scan after a nonmatching English subtitle (0xb776a). Native remained inside the first reference and could report bad syntax after scanning to EOF.

Field parsing now reports the complete invalid identifier and rejects an empty first character, matching 0xb795c/0xb7b11 and their continuations. The LFE clamp follows the subtraction and ordered-selection operations at 0xb7a2b-0xb7a62, including conversion of NaN and signed zero to positive zero.

Localization now parses directly after the handled newline (0xb882d), exports unset fields as empty (0xb8bbc), and reuses an existing subtitle-text reference without rewriting StringEd or increasing the new-subtitle count (0xb8ecd-0xb8f36). Reference conversion occurs when exporting the subtitle column, preserving Mac file-operation order. Its StringEd writer retains the append-on-EOF path at 0xb9104 and skips the line after ungetting the next marker at 0xb9233. The inline helper rows record the same corrections.

The complete remaining paths match: recursive stable sorting and duplicate removal, filename/type warning arguments, volume group loading and lookup, CSV parsing/validation, permanent allocation and field/flag construction, falloff-curve parsing and endpoint repair, reference lookup, CSV quoting and final file copying. All 24 dispatch targets were decoded from 0x32a620. The 24 alias-key strings at 0x348780 and 11 channel strings at 0x3487e0 match the native tables exactly. Mac's primed-to-streamed field mapping is preserved.

## Intentional native behavior

Native retains deterministic initialization of unused widened build-record storage. Mac initializes individual fields and leaves some unused record bytes indeterminate; no indeterminate native storage was introduced. Permanent allocations are zeroed by `Hunk_AllocInternal`, so the extra explicit clearing in native does not change their initial contents.

Dedicated builds omit client sample loading. The client engine loader was still compared against the complete Mac 0xb7590-0xb7657 body: loaded-file dispatch, missing count, streamed file touching, dvar gates and diagnostics match. Its native Miles replacement remains the documented `macos_audio.c` backend from WS3; excluding that backend does not exclude engine alias parsing, loading or selection.

## Verification and limits

The arm64 Release dedicated target built with two low-priority jobs, and both affected files also compiled using client flags. All five COD2_X64=OFF preprocessing configurations match the baseline. Neither changed file uses `__LINE__`. Triage and constant checks preceded hand comparison. `Com_InitSoundAlias` was disassembled again at its exact symbol to correct a misaligned full-binary entry.

No new tests, full CI or replays were run. Corrections are compiled and reviewed, not exercised. Runtime validation belongs to integration. No entries in this module remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-sound.noindex/`.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
