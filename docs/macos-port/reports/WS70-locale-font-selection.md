# WS70 — Original locale-specific menu fonts

2026-10-07, base `0f07adc`, branch `codex/locale-font-fidelity`.

The native menu asset parser now selects the original language-specific font
paths for languages 6–12 and 14. Previously every language registered the menu's
supplied default path. The ordinary font role uses `normalFont`; the other five
roles retain the menu keyword, including its spelling. The existing duplication
aliases and image-track argument are preserved. Languages 0–5, 13 and unknown
values keep the supplied font path. Stock and CoD2x share this correction.

## Independent reference

The owned Mac 1.3 binary's `RegisterFontHack` at `0x1a8acc` was inspected directly.
Its jump table at `0x33bec0` and referenced strings establish this mapping:

| Language | Directory |
| --- | --- |
| 6 | russian |
| 7 | polish |
| 8 | korean |
| 9 | taiwanese |
| 10 | japanese |
| 11 | chinese |
| 12 | thai |
| 13 | supplied path |
| 14 | czech |

`UI_ParseMenuInternal` passes the literal `normalFont` at `0x1acc67`, then the
current keyword for the other roles at `0x1acd24`, `0x1acdc8`, `0x1ace6c`,
`0x1acf2f` and `0x1acff2`. The helper formats into 256 bytes, then forwards the
same image track to `CL_RegisterFont`. This implementation uses a literal `%s`
format for the supplied default path, retaining safe handling of path text.
No disassembly, fonts or other licensed assets are included in git.

## Verification and limits

The fixture runs the actual font asset parser and global asset dispatch with
synthetic token/font dependencies. It covers all six roles for languages -1
through 15, six real dispatch branches, registration/duplicate arguments, and
missing path/point-size failure propagation. Its final 108 cases reproduce
54 failures against the starting production revision and zero failures under
stock and CoD2x assertions plus AddressSanitizer.

Both actual Release translation units compile. The inactive source checks pass
all ten fixes13 configurations and five generic native-off configurations; the
actual CoD2x SDK preprocessing command with COD2_X64 removed is identical.
An intermediate conditional-argument spelling introduced inactive whitespace
differences; complete gated signatures/call lines restored exact preprocessing
without changing the checkers. `git diff --check` passes.

Run `python3 tests/fixes13/run.py --build <build> --test locale_fonts`; add
`--baseline 0f07adc` for the prior behavior. Private evidence is under
`~/Library/Application Support/CoD2-native-ws63/validation.noindex/locale-*`.
These checks establish registration selection, not presence of every localized
asset, rendered glyph coverage, full menu parsing or a live locale screenshot.
Those remain composed runtime acceptance work. No game or global locale setting
was changed for this unit.
