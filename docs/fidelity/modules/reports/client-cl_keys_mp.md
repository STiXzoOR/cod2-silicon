# client/cl_keys_mp module reconciliation

Updated 2026-11-04.

**25/25 reconciled: 20 equal, 3 fixed, 2 intentional.** Mac 1.3 i386 is the authority. All assigned functions and the active keybinding, clipboard, editing and event helpers were compared. The OFF source text is preserved.

## Corrections

**The key event dispatcher omitted active branches.** `CL_KeyEvent` at 0x1412e2 now preserves first-press `devgui` routing, the restricted UI repeat mask 0x603, console/Escape repeat suppression, cinematic/demo cancellation and `nextdemo` clearing, game-client interception on key release, connecting-state cancellation and the local-server kill flag, server-load menu selection, and download cancellation before returning to the main menu. The Escape jump table at 0x33ade0 was decoded from the binary.

Shift+console now checks the Shift key's down state at 0x1414f1; native checked the grave key's binding pointer. An already active console uses the output toggle at 0x14150c. Empty binding strings retain the Mac command/newline path instead of being reported as NULL/unbound bindings. The native key-range guard, gamepad ownership notification, and existing bypass-gated UI query remain.

**Ctrl+Left skipped punctuation that Mac leaves in place.** Mac 0x1403ee–0x140468 moves left once and consumes preceding alphanumeric characters. Native had an additional punctuation loop. That extra loop is now confined to the preserved legacy branch.

**A field scroll retry used the wrong comparison.** At 0x13f4a2, Mac compares zero with the available line width when the cursor-width difference is negative at scroll zero. Native compared the cursor width and fell through. The zero comparison and nonpositive-width retry now match the binary, including its unusual retry behavior.

## Other comparisons and native boundaries

Scripts compared all 98 ordinary and 98 localized key-name/key-number pairs, plus all ten French digit mappings, with the Mac data; all match. Binding parsing, writing, quote escaping, completion, chat routing, character insertion and drawing were compared with their Mac callers, including inlined helpers. The actual UI, game-client interception, console and download implementations were traced before adding calls. Download status uses the existing optional-feature header, so builds with web downloads disabled retain their adapter.

The Mac `anykeydown` behavior is an unsafe pointer quirk. Its initialized pointer at 0x34acbc points to `playerKeys + 288`; exact-symbol disassembly confirms that 0x1415ad adds four to the pointer and 0x141474 subtracts four. `Key_ClearStates` at 0x141904 loads that pointer and then writes through it. Native preserves stable pointed counter storage and saturating releases rather than drifting through key/control fields. The two affected verdicts describe this adaptation.

The active macOS client and dedicated builds already use generated typed key storage. The aliases derive the 296-byte LP64 key-array offset, with a 4,392-byte, eight-byte-aligned `PlayerKeyState`; Clang's actual layout confirms these values. Raw blob-source comments and initializers still mention the old 292-byte offset, but those are replaced by the generated native boundary. No shared storage change was needed for this reconciliation.

`PbMaxKeys` at 0x13f9da returns 256 in Mac but has no native engine callers. It belongs to the discarded PunkBuster callback backend, replaced by `src/PC/punkbuster/pb_null_adapter.c`, and remains intentionally absent. The `devgui` binding branch queues the command through live `Cbuf_AddText`; this work does not add a native developer GUI command, which currently has no registration.

## Verification and limits

The arm64 Release dedicated build succeeded. Both owned client sources compiled directly with client flags, and this module also compiled with web downloads disabled. The existing UI key-routing fixture passed; the existing nine-group stance fixture passed. Five OFF preprocessing configurations match the base, and neither changed source uses `__LINE__`. No tests were added or changed, and no full CI or replay was run.

The restored dispatcher branches, Ctrl+Left change and nonpositive-width scroll retry were compiled and reviewed but not exercised individually. The UI fixture exercises the existing bypass routing slice, not the complete event dispatcher. Parent integration owns runtime validation.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-input.noindex/`. The complete per-entry verdicts are in the private `client-input.ledger.json` unit ledger.
