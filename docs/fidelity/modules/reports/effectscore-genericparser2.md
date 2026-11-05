# EffectsCore/GenericParser2 module reconciliation

Updated 2026-11-05.

**45/56 reconciled: 42 equal, 1 fixed, 2 intentional; 11 unchecked.** Mac 1.3 i386 is the authority. All 14 exported bodies were read from disassembly restarted at their exact symbols. Inline operations were compared in their Mac callers; missing Mac identities were not counted as equal. Linux declarations were used for names only. No reference code was copied.

## Correction

**The cleanup flag used too many bits.** Mac `GenericParser2::Parse` at 0xac257 tests the low byte of `cleanFirst`. Native accepted an `int` and tested every bit, so an argument such as 256 cleaned existing parser state. The native branch now tests a byte cast. OFF source behavior is unchanged. The arm64 Release binary contains `tst w2, #0xff` at 0x1000c28e4.

## Ownership and native storage

Names and values are copied into a text pool when a pool pointer is supplied. Calls without a pool retain their supplied string pointers. `GPGroup_Clean` frees list nodes and subgroups without freeing individual strings; parser destruction frees pool data and pool nodes after cleaning the groups. The two Mac parser destructors perform a second group clean, which native also retains. Pool extension preserves the current capacity, links one new pool, and retries once without permission to extend again.

Native `GPObject`, `GPValue`, `GPGroup`, `GenericParser2` and `TextPool` use pointer-sized fields and `sizeof` allocations. Their LP64 sizes are 32, 40, 96, 112 and 24 bytes. Mac uses 16, 20, 48, 56 and 16 bytes. Capacity and used counts remain integers. The allocator zeroing contract was checked in exact Mac `Z_MallocInternal` at 0x37d4c and native `com_memory.c`.

Mac `GPValue::AddValue` allocates a 16-byte `GPObject` for each value string at 0xabe76 and 0xabeb5. Native already allocates a full 40-byte `GPValue`, rather than a 32-byte native `GPObject`. This larger typed storage is retained intentionally. Consumers use the shared name and link fields; the head's `sortedNext` field acts as the value-list tail. The native allocation was not narrowed to reproduce absent storage.

The Mac null-parent behavior is retained: `AddGroup` initializes the parent to null, and parsing a subgroup does not assign a parent. Consequently an otherwise defined end-of-input can succeed inside a newly parsed group. Stable case-insensitive sorting and the insertion-order chain were compared separately.

## Unresolved boundaries

Historical GetToken gap, resolved by the native-boundary follow-up below: Its whitespace, comment, quote, size-limit and end-of-line trimming branches were compared, but Mac 0xab95f-0xab999 does not stop an end-of-line read at NUL. An unterminated quoted token also advances its cursor beyond NUL at 0xaba0f-0xaba1f, leaving the next scan outside the declared input.

The real loader is inline in Mac `FX_ParseEffect`: 0x5d25f requests `fileLength + 1`, 0x5d273 supplies `fileLength` to `FS_Read`, 0x5d28a appends one NUL, and 0x5d2ab calls the parser. Native `FX_ParseEffectFile` has the same boundary and adds no newline. Mac `Hunk_AllocateTempMemoryInternal` at 0x38192 reserves a header before the requested extent, with no readable suffix guarantee; its fallback requests the exact size from `malloc`. Neither caller nor allocator establishes a safe scan past the final NUL. Existing native bounds remain unchanged; no padding or indeterminate storage was introduced. Parser control-flow verdicts cover inputs whose token reads remain within storage.

The other 11 unchecked rows are `GPValue::Duplicate`, `GPGroup::Duplicate`, `FindPair`, `FindPairValue`, `FindSubGroup`, `GetNumPairs`, `GetNumSubGroups`, the three `Write` overloads, and `WriteText`. No callable Mac body or identified inline caller was found for these Linux-only identities. Native has no standalone implementation for them, so their behavior is unresolved.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled successfully using `taskpolicy -b nice -n 19` and two jobs. The existing dedicated FX fixture passed under AddressSanitizer: parsing, 450 ms lifetime, cache cleanup, channels and media growth. No tests were added or changed. OFF preprocessing matched the assigned baseline in all ten configurations checked by `tests/lp64/game/check_legacy.py`; this source contains no `__LINE__` expansions. `git diff --check` passed.

The upper-byte-only cleanup argument is compiled and reviewed, not exercised. Malformed EOF, pool exhaustion, oversized public string arguments, allocation failure, and unresolved APIs were not exercised. No full CI, replay, Windows comparison, push or shared-header edit was performed.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-fx-parser.noindex/`. The private unit ledger records all 56 assigned IDs.

## Integrated batch verification

Integrated source cef44cb passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.


## Defined native boundary follow-up

See [native-invalid-boundaries](native-invalid-boundaries.md) for the exact native contracts, original addresses, source corrections and limits. Earlier open statements for the six named Mac rows describe the prior checkpoint. Current module counts are 42 equal, 1 fixed, 2 intentional, 11 unchecked. All other original identity gaps remain open.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.



## Remaining API identity follow-up

The eleven open rows were investigated again from integration `89e09ab8`. **No additional row is reconciled: 42 equal, 1 fixed, 2 intentional, 11 unchecked.** All 45 resolved status/evidence records are preserved exactly, including the intentional `GetToken` malformed-EOF boundary. Production source and fixtures are unchanged.

The full Mac symbol table contains STABS declarations for both `Duplicate` methods, all three lookups, both counts, the three `Write` methods and `GPObject::WriteText`. Thus “Linux-only” above describes their executable ledger identities, not an absence of Mac declarations. The Mac signatures include the parent argument of `GPGroup::Duplicate(TextPool **, GPGroup *)` and distinguish the parser's `Write(TextPool *)` from the group's/value's `Write(TextPool **, int)`. None of these declarations provides a body. Neither a callable symbol nor a complete inline operation was established. The two count methods have absolute-zero `.eh` labels, which are not executable addresses.

Mac type records place the four `GPObject` fields at byte offsets 0/4/8/12, the value list at 16, the group's pair lists at 16/20/24, subgroup lists at 28/32/36, parent at 40 and writeable byte at 44. The parser embeds its 48-byte top-level group at zero and adds pool/writeable storage. These nonvirtual layouts have no vptr; no parser vtable/typeinfo symbols supplied another dispatch route. Native ownership is the corresponding pointer-sized structure in `common_types.h` and the APIs in `fx_funcs.h`/`GenericParser2.c`.

Raw Mach-O STABS inspection collected 399 relevant function/source/line records (271 attributed to the `.cpp`, 128 to the header). Source-line numbers were used with their Mac owner, not equated to Linux line numbers: for example Mac line 505 names `GPGroup::Clean` at 0xabb66, and Mac lines 465–466 occur in destructor cleanup. Such records do not identify the Linux count/duplicate rows. The header-attributed operations in FX consumers were inspected in exact-entry disassembly:

- `FX_ParseEffect` loads the subgroup head at 0x5d302, advances its link at 0x5d381, and dispatches each name at 0x5d392. The accepted-primitive count and traversal index at 0x5d368–0x5d38a belong to effect construction; they are not complete `GetNumSubGroups`/`GetNumPairs` methods.
- `PrimitiveTemplate::ParseChannel` loads the pair head at 0x5e8d9–0x5e8dc and advances at 0x5e93c/0x5e9b3. `ParsePrimitiveInternal` loads pairs at 0x5f781–0x5f784 and advances at 0x5f85a; its subgroup pass loads at 0x5fa53–0x5fa56 and advances at 0x5fb4b. Native uses the same list owners. These passes process all nodes and dispatch their values; they do not establish the missing name lookup and default-value contracts.
- `FxArchive::ArchiveEffect` at 0xaa208–0xaa26c writes a length byte and the effect's name through `FxArchive::WriteData`. Native `FxArchive_ArchiveEffect` has that same distinct ownership. This is not parser tree serialization and does not establish `Write` or `WriteText`. Allocation and string copying in `TextPool::AllocText` likewise do not establish text formatting or recursive duplication.

No documented selected native tooling replacement was found for these eleven APIs. Their absence from live native owners cannot justify an intentional verdict. Quoting/indentation and writer failures, recursive copy ownership, lookup ordering/defaults, and complete count behavior remain unproved. No helper or caller was invented to close a row, and no Linux implementation was used as authority or copied.

The configured stock and CoD2x arm64 Release `GenericParser2.c` objects both compile using integration's recorded engine flags redirected to this worktree and private output files. The unchanged `tests/platform/run_dedicated_fx.sh` fixture passes under ASan (parsing, 450 ms lifetime, cache, channels and media growth). Builds ran under `taskpolicy -b nice -n 19` with at most two compiler jobs. Call/string triage and a ranged getter constant check were used only as heuristics; neither establishes any new identity. `git diff --check` passes. No source changed, so the existing OFF source and `__LINE__` behavior are unchanged. No new tests, cases or assertions, full CI, replay, Windows comparison, push or shared-header edit occurred. The eleven absent implementations were not exercised; object compilation and the live parser fixture do not validate them.

Private evidence and neutral review scope: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-generic-parser-remaining.noindex/`. The unit ledger contains all 56 module rows, including the unchanged resolved records and expanded evidence for the eleven unchecked rows.
