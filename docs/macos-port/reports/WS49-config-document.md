# WS49 — Config Studio byte document (F1)

Implemented the F1 unit from `docs/superpowers/plans/2026-10-07-config-studio-foundation.md` on `codex/config-document`, starting at `62135bb`. The new Foundation-only model does not alter launcher integration, build lists or engine code. Changes remain uncommitted for the orchestrator.

## Consumer interface

- `ConfigDocument(data: Data, dialect: .stock | .cod2x)` and `serialized()` preserve raw bytes, including Latin-1, BOM, mixed endings and the entire suffix beginning at the first NUL.
- `commands` exposes command byte ranges, separator/discarded-byte ranges, token values and ranges, quote closure, comment ranges, tokenization failures and command-limit splitting. Ranges are zero-based byte offsets in `serialized()`, not Unicode character offsets. Tokens expose a one-byte-to-one-scalar `text` view; `value` remains `Data`.
- `diagnostics` reports ignored NUL suffixes, command-buffer overflow, line limits, token failures and unterminated quotes. Consumers must inspect these before describing a script as executable or safe. `ignoredSuffixRange` makes the opaque tail available without parsing it.
- `effectiveSetting(named:)` / `effectiveSettingBytes(named:)` look up direct `set`, `seta`, `sets`, `setu` and queried bare-dvar assignments, case-insensitively. Duplicate assignments are last-wins. A later `reset`, `toggle`, `togglep` or `setfromdvar` targeting the dvar makes its static value unresolved.
- `effectiveBind(key:)` / `effectiveBindBytes(key:)` replay direct `bind`, `unbind` and `unbindall`, including key-number aliases, argument reassembly and the 1023-byte binding limit. Binding keys fold ASCII letters; unbind keys retain the engine's case-sensitive one-byte behavior. The 98 named keys and engine hex-reading rules are represented.
- `setSetting(named:value:)` and `setBind(key:binding:)` accept `String` or `Data` and throw meaningful `ConfigEditError`s. String edits reject scalars outside U+0000–U+00FF rather than transcoding existing bytes. Setting values with quotes, line breaks, NUL or trailing backslash are refused. Binds also enforce 1023 bytes and the splitter's quote-parity rules. A successful change reparses and verifies the requested token values and every unaffected command's exact bytes and tokens.
- `removeSetting(named:)` removes all direct assignments to prevent an earlier duplicate from reappearing. `removeBind(key:)` removes all bind assignments for that normalized key. Script command separators remain; canonical entries lose their whole physical line. Runtime-dependent commands remain untouched.
- `isCanonical` validates the whole file: exact generated header, two immediate `unbindall` lines, only well-formed binds followed by seta lines, strict key/name ordering, no duplicates, extra spacing, comments, malformed quotes, NUL tail or parser diagnostics. Canonical insertions use sorted positions; other scripts append missing entries before the NUL suffix. New lines use the first physical line ending, or LF for a file without one.
- `unresolvedCommands` exposes `exec`, `vstr`, `toggle`, `togglep`, `reset` and `setfromdvar` with their original tokens and spans. Lookups are direct document evidence. They do not execute dependencies, simulate chains, apply dvar types/protection/server policy or promise the final engine state. Future import and chain units own that analysis.
- `ConfigDocument.tokenize(data:dialect:tokenLimit:byteOffset:)` separately models direct tokenizer calls, including the verbatim final remainder of limited-token calls. Normal document parsing uses the command-buffer splitter first.

No runtime launch, profile filesystem transaction, safety classification, chain VM, key press/release simulation or UI was added in F1. Those belong to the dependent units. Files exceeding engine limits still round-trip; edits refuse newly lossy or overflowing output. Existing long commands retain their parsed truncation/discard evidence.

## Verification

Test discovery inspected the existing launcher core/artwork fixture style and the corrected WS47 `cmd.c`, `cod2x_policy.c`, `dvar_cmds.c` and `tests/fidelity/tokenizer.c`, plus the machine-local grammar/roundtrip evidence. There was no existing config-document production model or test to reuse.

Proof-first evidence: the first runner failed because `launcher/ConfigDocument.swift` did not exist. After implementation, a new regression test for inserting a setting ending in a backslash failed with `FAIL: lossy edit accepted` before the production correction. Validation and insertion's comparison against the requested bytes fixed that failure. The focused suite now proves effective edits and exact unrelated bytes, duplicate preservation/removal, canonical versus script insertion, safe and unsafe quoting, comments, signed-byte whitespace, BOM/NUL/CRLF, token limits, line-discard behavior and engine buffer/bind limits.

Commands and observed results:

```sh
sh tests/launcher/config_document.sh
# PASS: focused synthetic production-model tests

sh tests/launcher/config_document.sh --grammar "$PRIVATE_GRAMMAR_FIXTURES" \
  --corpus "$PRIVATE_WINDOWS_CORPUS" "$PRIVATE_GRAMMAR_FILES" "$PRIVATE_WEB_CONFIGS"
# PASS: 75 independent grammar checks
# PASS: 113 files, 1,406,497 bytes, exact round trip in both dialects

taskpolicy -b nice -n 19 python3 /tmp/cod2-config-oracle.VQ6DKt/run.py
# PASS: 20,320 comparisons with the corrected production C tokenizer
# 10,160 synthetic randomized/boundary cases per native stock/CoD2x mode; zero mismatches

git diff --check
# PASS
```

The focused runner compiles with Swift 6, complete strict concurrency, warnings as errors, optimization and an arm64/macOS 13 deployment target. Builds run at background priority and use only temporary output, with no packages, global configuration or game runtime. The private corpus comprises 34 Windows profile/selection files, eight grammar whole-file fixtures and 71 web configs. Verification prints counts only and copies no profiles into git. The independent grammar files remain machine-local; CI uses the authored synthetic cases in `ConfigDocumentTests.swift`.

The scratch C comparison extracts only current MIT production functions, compiles them in both native modes and compares token bytes with the Swift entry point. It does not include private binaries, extracted assets, decompiler code or profile data. The long initial setup command was rejected because the local safety hook's analysis failed; restructuring setup as small file writes and a simple execution succeeded.

These are model/fixture checks, not end-to-end launch or game tests. Launcher integration and the orchestrator's authoritative composed verification remain outstanding. Requested worker route/model: native Codex, Sol 6.1 xhigh; no separate served-model receipt was available to this worker. No canonical commits or standalone shipping actions were performed.
