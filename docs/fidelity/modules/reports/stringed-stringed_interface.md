# stringed/stringed_interface reconciliation

Updated 2026-11-05.

**4 entries: 2 equal, 2 intentional.** All four exact Mac 1.3 i386 bodies were compared by hand. Source changes were unnecessary.

LoadFileData at 0xbc8bc returns the file buffer only for a positive FS_ReadFile length, and FreeFileDataAfterLoad at 0xbc8e2 delegates directly to FS_FreeFile. Recursive listing at 0xbc8ec preserves directory filtering, directory-before-file order, path formatting, per-file delimiter/count and file-list release order. BuildFileList at 0xbca88 clears the destination/counter and uses the str extension.

The existing native Apple implementation deliberately uses the documented 8-byte Cod2MacString and 24-byte COW representation, with append(";", 1) replacing original i386 -12/-8/-4 header writes. The shared provider's size_t and detach contracts were inspected read-only.

The Release arm64 dedicated build links, and the actual client translation unit compiles with this worktree's production flags. The existing ASan/UBSan cpp_abi fixture passes actual SE_BuildFileList recursion, delimiters, count and COW detachment, together with its existing provider tests. It does not validate the original fixed 64-byte path buffer on long paths or game filesystem assets. No new cases, full game run or replay were added. Private logs and Mac evidence remain under recon-stringed.noindex.

## Integrated batch verification

Integrated production source 02a2156 plus existing-fixture correction ac63727 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. All five guard-visible production changes and the existing numeric-field fixture differ only under native port-on; the Apple-only StringEd source has no inactive differences. Actual assertion line expansions in both UI source files match the previous source in all five inactive configurations. Step 13 first failed because the existing UI text-color extraction counted both conditional opening braces and omitted the function; the fixture now preprocesses it using engine flags, as two other existing suites already do. All unchanged online fixtures passed in stock and CoD2x, followed by the complete source-stage retry. Original failure evidence is retained. No new tests were added. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units, including the corrected early-download text path. Fourteen entries remain unchecked: thirteen StringEd identities and one UI missing-player string boundary. Localization parser/tree exception cleanup and unreached client UI/audio paths remain compiled and reviewed; the existing fixtures do not establish complete retail asset behavior. Group voice preserves the documented playback-only native capture boundary.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. The first stock attempt stopped before engine launch because previous output existed; verified backed-up output was cleared and the complete replay passed. Dedicated replay does not exercise UI interaction, client sound output or microphone capture.

Catalog coverage passed without semantic catalog changes. The predecessor batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run is incomplete. Hosted ILP32 comparison remains unavailable because the unchanged reference fails compilation; no byte parity is claimed.
