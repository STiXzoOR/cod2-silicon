# EffectsCore/GenericParser2 module reconciliation

Updated 2026-11-05.

**44/56 reconciled: 42 equal, 1 fixed, 1 intentional; 12 remain unchecked.** Mac 1.3 i386 is the authority. All 14 exported bodies were read from disassembly restarted at their exact symbols. Inline operations were compared in their Mac callers; missing Mac identities were not counted as equal. Linux declarations were used for names only. No reference code was copied.

## Correction

**The cleanup flag used too many bits.** Mac `GenericParser2::Parse` at 0xac257 tests the low byte of `cleanFirst`. Native accepted an `int` and tested every bit, so an argument such as 256 cleaned existing parser state. The native branch now tests a byte cast. OFF source behavior is unchanged. The arm64 Release binary contains `tst w2, #0xff` at 0x1000c28e4.

## Ownership and native storage

Names and values are copied into a text pool when a pool pointer is supplied. Calls without a pool retain their supplied string pointers. `GPGroup_Clean` frees list nodes and subgroups without freeing individual strings; parser destruction frees pool data and pool nodes after cleaning the groups. The two Mac parser destructors perform a second group clean, which native also retains. Pool extension preserves the current capacity, links one new pool, and retries once without permission to extend again.

Native `GPObject`, `GPValue`, `GPGroup`, `GenericParser2` and `TextPool` use pointer-sized fields and `sizeof` allocations. Their LP64 sizes are 32, 40, 96, 112 and 24 bytes. Mac uses 16, 20, 48, 56 and 16 bytes. Capacity and used counts remain integers. The allocator zeroing contract was checked in exact Mac `Z_MallocInternal` at 0x37d4c and native `com_memory.c`.

Mac `GPValue::AddValue` allocates a 16-byte `GPObject` for each value string at 0xabe76 and 0xabeb5. Native already allocates a full 40-byte `GPValue`, rather than a 32-byte native `GPObject`. This larger typed storage is retained intentionally. Consumers use the shared name and link fields; the head's `sortedNext` field acts as the value-list tail. The native allocation was not narrowed to reproduce absent storage.

The Mac null-parent behavior is retained: `AddGroup` initializes the parent to null, and parsing a subgroup does not assign a parent. Consequently an otherwise defined end-of-input can succeed inside a newly parsed group. Stable case-insensitive sorting and the insertion-order chain were compared separately.

## Unresolved boundaries

`GetToken` remains unchecked for its full input domain. Its whitespace, comment, quote, size-limit and end-of-line trimming branches were compared, but Mac 0xab95f-0xab999 does not stop an end-of-line read at NUL. An unterminated quoted token also advances its cursor beyond NUL at 0xaba0f-0xaba1f, leaving the next scan outside the declared input.

The real loader is inline in Mac `FX_ParseEffect`: 0x5d25f requests `fileLength + 1`, 0x5d273 supplies `fileLength` to `FS_Read`, 0x5d28a appends one NUL, and 0x5d2ab calls the parser. Native `FX_ParseEffectFile` has the same boundary and adds no newline. Mac `Hunk_AllocateTempMemoryInternal` at 0x38192 reserves a header before the requested extent, with no readable suffix guarantee; its fallback requests the exact size from `malloc`. Neither caller nor allocator establishes a safe scan past the final NUL. Existing native bounds remain unchanged; no padding or indeterminate storage was introduced. Parser control-flow verdicts cover inputs whose token reads remain within storage.

The other 11 unchecked rows are `GPValue::Duplicate`, `GPGroup::Duplicate`, `FindPair`, `FindPairValue`, `FindSubGroup`, `GetNumPairs`, `GetNumSubGroups`, the three `Write` overloads, and `WriteText`. No callable Mac body or identified inline caller was found for these Linux-only identities. Native has no standalone implementation for them, so their behavior is unresolved.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled successfully using `taskpolicy -b nice -n 19` and two jobs. The existing dedicated FX fixture passed under AddressSanitizer: parsing, 450 ms lifetime, cache cleanup, channels and media growth. No tests were added or changed. OFF preprocessing matched the assigned baseline in all ten configurations checked by `tests/lp64/game/check_legacy.py`; this source contains no `__LINE__` expansions. `git diff --check` passed.

The upper-byte-only cleanup argument is compiled and reviewed, not exercised. Malformed EOF, pool exhaustion, oversized public string arguments, allocation failure, and unresolved APIs were not exercised. No full CI, replay, Windows comparison, push or shared-header edit was performed.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-fx-parser.noindex/`. The private unit ledger records all 56 assigned IDs.
