# A0 — owned CoD2 binary inventory

2026-10-15. The user authorized local Rizin, REA and Ghidra provisioning and
starting private decompilation. Completed phases must be committed and pushed.
Requested model preference for this work is Sol 6.1 at xhigh. Model selection
must be verified in the host; the assistant cannot change the current chat's
model through available tools.

U1 is committed as `9f7249b`. This phase inventories known reference/game roots
and selects three owned Mac i386 inputs. It does not claim coverage of every
CoD2 release or completion of whole-program analysis.

| Selected target | SHA-256 | Bytes | Symbol entries | STABS-marked entries |
| --- | --- | ---: | ---: | ---: |
| Mac MP 1.3 reference | `15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12` | 20,211,172 | 414,540 | 396,988 |
| Owned Steam Mac SP | `9c4592fd0bc6d2d04d82162df52cbad3b318c01fd919fbb96d6fa3399adf6a0a` | 4,697,200 | 40,086 | 30,804 |
| Owned Steam Mac MP | `a6aa70d4e2b0bf5f388b0653752847bddac95398ee0d860e757ec79e466f5d1b` | 4,958,488 | 43,829 | 33,586 |

Counts come from each Mach-O LC_SYMTAB and nlist type bits. STABS-marked entries
are not a count of recoverable type definitions or functions. In particular,
the Steam files' records do not supersede the established datagen type source.
The private manifest also records load commands, segment addresses/protections,
file extents and UUID where available. Both MP inputs were independently opened
by Rizin as x86/32-bit Mach-O at base 0x1000.

The bounded scan covered the existing `cod2-native-refs` and `CoD2-mac-bin`
roots: 718 executable/library candidate entries, including many tooling,
Wine/runtime and prior port build artifacts. These are not 718 game binaries.
Six additional game-named candidates are deferred for provenance/version review;
they include Windows SP/MP, Linux dedicated variants and a universal Mac image.
Duplicate paths do not imply unique binaries. Helpers, libraries and port builds
are excluded from the initial reconstruction corpus.

All three selected originals were SHA-256 checked and copied to read-only files
in the private analysis root outside Git. No original was modified or executed.
The scanner excludes symlinks and non-regular files (143 skipped entries);
a first scan was stopped after blocking on a non-regular file, then rerun with
that filter. No analysis result is inferred from the interrupted scan.

Machine-local evidence: `~/Library/Application Support/CoD2-native-binary-analysis/20261015.noindex/receipts/binary-manifest.json`.
The manifest, binaries, disassembly, pseudocode and provider projects remain
private. Only this sanitized summary is committed. A1 will validate providers
against the existing WS75 water and WS94 lighting contracts before bulk export.
