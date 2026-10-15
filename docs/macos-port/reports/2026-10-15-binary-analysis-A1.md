# A1 — local analysis tools and bounded decompilation pilot

2026-10-15. The user authorized local tool installation and private analysis.
All installations, input copies, provider snapshots, exports and logs are under
`~/Library/Application Support/CoD2-native-binary-analysis/20261015.noindex/`,
outside the repository. No game executable was launched. Global PATH, system
packages and Codex's MCP configuration were not changed.

## Pinned local toolchain

| Component | Version and validation |
| --- | --- |
| Rizin | 0.9.1, macOS arm64 package; published SHA-256 `949f56d56698bdad2f39996badc10d0ada13033fad5fc7ef4fe5eaa89d945e94` verified. |
| REA | 6.5.0, private npm prefix; exact version and registry SHA-512 integrity match the lockfile. Installed with lifecycle scripts disabled. |
| Ghidra | 12.1.4; release ZIP SHA-256 `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db` verified. |
| Ghidra native decompiler | Built from the release's included source for macOS arm64; SHA-256 `ddd22069030800743d3e2ffa1200ecfde8ad1cd0fc823706de5224c8c234d44c`. |
| Java | Local Temurin JDK 25.0.4.1+1 arm64; archive SHA-256 `61979887f7506a24a57439ff99adb8b3a7fc89977d9cfe3b8984f58a981b7b9d` verified. Ghidra 12.1.4 requires JDK 21 or newer. |
| JavaScript runtime | Existing Node 24.21.0 and npm 11.19.0. |

Sources: [Rizin release](https://github.com/rizinorg/rizin/releases/tag/v0.9.1),
[Ghidra release](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.4_build),
[REA](https://github.com/morluto/rea), and
[Temurin release](https://github.com/adoptium/temurin25-binaries/releases/tag/jdk-25.0.4.1%2B1).
Local `receipts/toolchain.json` binds package identities, commands and environment.

Rizin's package was expanded rather than installed into `/usr/local`. A private
launcher sets the package library path in its own process. Direct invocation
through macOS system scheduling wrappers initially lost the DYLD environment;
the launcher fixes that without changing the binary or global library paths.

Ghidra's ZIP supplied no macOS ARM native decompiler. Its included C++ source
was built with `make -j3 ghidra_opt`, ARM64 and macOS 26 flags, under
`taskpolicy -b nice -n 19`. The executable was placed in the local installation's
`Ghidra/Features/Decompiler/os/mac_arm_64/`. No source was copied into the port.

The private `bin/rea` launcher supplies exact Ghidra, JDK, Rizin and scratch paths.
Scoped `rea doctor --provider ghidra` exits zero with all ten required checks
passing. Unconfigured optional providers and agent registrations are not required
for this CLI/local-stdio-MCP route. A local MCP initialization/tool-schema probe
also succeeded; it does not register a global connector.

## Initial evidence

REA/Rizin returned bounded disassembly at the known WS94 lighting and WS75 water
addresses with exit zero, no provider truncation and unchanged input SHA-256.
REA/Ghidra returned pseudocode for `R_LoadWorld` at `0xe1d3c`, bound to the exact
Mac MP 1.3 reference hash and Ghidra 12.1.4. Its seven lighting destinations and
world-relative field accesses agree with WS94's sun-parse-relative contract
(the world-to-sun-parse offset is 0x34). Rizin independently observes the seven
sun-parse-relative offsets. These are bounded semantic checks, not gameplay tests.

The output still includes unknown types. Successful decompilation does not
establish correct prototypes, complete type recovery, indirect-call resolution
or original source equivalence. Only abstract findings and sanitized counts
belong in this report; raw assembly and pseudocode remain private.

## Completed pilot batch

A reusable local REA stdio MCP session completed 22 operations and closed cleanly.
Six functions have private pseudocode, complete provider-returned assembly and
function metadata: `R_LoadWorld`, `RB_UploadWaterTexture`,
`RB_ChangeGenTexCoords`, `RB_SetVertexData`, vertex-buffer Lock, and
`Player_GetUseList`. The snapshot and per-operation hashes are retained privately.
The six successful exports are the first bounded A3-style batch, not a complete
whole-program export or validation of every function's behavior.

Water checks agree with the established WS75 contracts: frame caching before the
enable check, clearing both components for zero frequency, the scene-time load,
and mode-2 sign reversal of the field at backend offset 0x4dc. Mode 1 uses the
inverse matrix. These checks validate selected observations; the four remaining
routines outside lighting/water have successful exports, not new behavioral
certification. All three selected originals and private copies still match their
A0 hashes after analysis.

A1's provider-readiness and bounded lighting/water pilot criteria are satisfied.
No runtime game test or Metal certification was performed.

## Discovery started

Rizin exported section and symbol inventories for all three A0 inputs. Raw
symbol counts are 49,025 for the old MP reference, 10,595 for Steam MP and 9,567
for Steam SP. Rizin labels every returned symbol `FUNC` on these inputs, so
these must not be reported as function counts.

An independent Mach-O section ledger distinguishes instruction attributes,
stubs and data/metadata. Executable segment permissions alone are insufficient:
constants, strings and unwind data can share executable mappings. The initial
instruction-attributed byte totals are 2,390,895, 2,436,010 and 2,330,362
respectively. These are input denominators, not analyzed/decompiled coverage.
Ghidra additionally returned 8,061 procedure records for the old MP reference:
7,427 internal functions and 634 external entries. It marks 633 records as
thunks (an overlapping classification, not an additional count). This inventory
is provider-discovered coverage, not proof that all executable code has been
found. Function-body reconciliation and unresolved-code accounting remain A2 work.

## Private usage and continuation

Use the private `bin/rizin` or `bin/rea` launchers with their ordinary upstream
arguments. `README.md` at the analysis root describes the layout. Originals and
copies are hashed; input copies have read-only permissions. The enclosing root
is owner-only. Captures, scratch projects and analysis output are never committed.

A2/A3 must retain explicit per-function failures, timeouts, source identities and
unresolved ranges; bulk export is not complete merely because a command exits
zero. Complete type/source mapping and behavioral validation remain A4–A6.
