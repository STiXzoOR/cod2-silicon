# Differential simulation parity

This is a **local server simulation** check, not a renderer, network-client or
full game correctness proof. It requires licensed data and working binaries.
The wrappers use Python 3's standard library; no packages are needed.

## Source contract

Read `src/unix/sysdiff_statehash.c`, upstream commit
`d1376001badda6595ff82d281a45d945c33a1d8e` ("tools: enhance differential
statehash for x86/x64 cross-architecture parity verification"). Hooks are in
`Com_Frame` (`src/PC/qcommon/common.c`) and the Mac main loop
(`src/Mac/Main/mac_main.c`). The duplicate-call guard emits once per positive
`level.framenum`. A remote client/demo does not run this local server state.
`Sys_DiffSeed` hooks `G_InitGame` and `G_LoadGame` in `g_main_mp.c`; it does not
promise to intercept every RNG or make live human/network input deterministic.

Each line is:

```text
frame 1 simt 50 ents 24 raw 01234567 q 89abcdef
```

These are illustrative values. `simt` is `level.time - level.startTime` and
`ents` counts in-use entities in ascending array order. CRC32 uses polynomial
`0xEDB88320`. `raw` hashes all bytes of each active `entityState_t`, including
padding, absolute times and potentially uninitialized type-specific fields.
It is diagnostic; do not demand raw equality between architectures/runs.

`q` hashes only number, eType, eFlags, pos.trType, apos.trType, and all three
components of pos/apos trBase/trDelta. Floats are rounded to signed int32 at
1/64 precision (half away from zero). Despite an older comment mentioning
weapons/animations, the implementation **omits** weapon, legsAnim, torsoAnim,
trDuration, trTime, time and time2. It excludes free slots, pointers and most
engine state (playerState, script VM, RNG state, physics internals). CRC collision
and quantization can hide differences. Hashes assume the same integer field
widths and little-endian representation (true for intended i386/x86_64/arm64).
Nonfinite or overflowing floats are outside this hash's valid input domain.
Keep `-ffp-contract=off` on the arm64 build per PLAN; do not change x86 flags.

Environment variables:

| Variable | Effect |
| --- | --- |
| `SYSDIFF_STATEHASH` | Output filename; absent disables hashing; open failure silently disables it |
| `SYSDIFF_SEED` | Cached `atoi` seed override; wrapper restricts to nonnegative int32 to avoid sentinel values |
| `SYSDIFF_MAXFRAMES` | Number of emitted records, not an absolute frame number; engine calls `_Exit(0)` at limit |
| `SYSDIFF_DUMP` | Per-entity text dump at that limit, with individual `eq` hashes |

## Record and compare

Use identical clean 1.3 data, localization, map, gametype, seed and cvars.
Avoid mods, connecting players, map rotation, auto-updates and extra configs
in the data tree. Fresh home directories prevent saved settings from drifting;
basepath autoexec/config content must still match. Each output directory must
be new. Output contains private local paths, hashes, logs and generated configs;
keep it under ignored `output/`, never commit it or upload game files.

```sh
# On x86 Linux, with an actually runnable 32-bit build:
python3 tools/parity/record.py --binary build-native/cod2_lnxded \
  --data /srv/cod2 --output output/x86-a --dedicated --frames 1000
python3 tools/parity/record.py --binary build-native/cod2_lnxded \
  --data /srv/cod2 --output output/x86-b --dedicated --frames 1000
python3 tools/parity/compare.py output/x86-a/statehash.txt output/x86-b/statehash.txt

# On the Mac after WS1/WS2/platform/runtime work lands; requires a GUI session:
python3 tools/parity/record.py --binary output/build-macos/cod2_macos \
  --data /path/to/cod2 --output output/arm64-a --frames 1000
# Transfer just reference trace/manifest/dump privately to this machine, then:
python3 tools/parity/compare.py output/x86-a/statehash.txt output/arm64-a/statehash.txt
```

Repeat the candidate against itself too. Fix same-build nondeterminism before
interpreting a cross-build mismatch. `run.json` records arguments, platform,
binary SHA256 and every `main/*.iwd` SHA256; compare manifests and note commits
and compiler versions. The wrapper fixes simulation steps to 50 ms at 20 Hz,
uses a loopback LAN server, and fails on timeout, crash, empty-world output,
missing dump or wrong record count. This minimal map smoke has no player input;
for movement/combat coverage a future deterministic bot/usercmd fixture is
needed. Dedicated-versus-listen differences must be ruled out on x86 first.

`compare.py` compares frame, simulation time, active count and normalized `q`.
It exits 0 for equality and nonzero otherwise, reporting the first differing
record/frame. Empty, malformed, missing, noncontiguous, duplicate/reset and
unequal-length traces cannot pass. `--raw` is only a stricter diagnostic.

## Locate and bisect divergence

1. Save the first failing traces, manifests and binary identifiers. The comparer
   gives record N and frame F. N and F need not be equal.
2. Rerun both with `--frames N`. Diff `entities.txt`; its `e<slot>` and `eq` identify
   the first differing entity. Text velocities/angles have lower precision than
   hashes, so identical printed decimals do not prove identical fields.
3. Rerun at N-1 to confirm the preceding state agrees. For long runs, binary
   search record limits: compare the **whole prefix** at each midpoint. Never
   compare only the last hash, since divergence can later reconverge.
4. At F, break in `G_RunFrame` / `Sys_StateHashFrame` in a debugger and inspect
   the first mismatching fields, then trace their writers. Do not change or
   commit production x86 instrumentation merely to obtain richer dumps.
5. For commit bisection, the orchestrator can use a separate authorized checkout:
   build each revision, run this same fixture and compare with the known-good
   trace. Use exit 125 for unbuildable revisions, 1 for a reproducible mismatch,
   0 for a complete matching run. Do not bisect a branch shared with workers.

Known upstream limitation: the array is declared as 1024 entities in `bss.c`,
while the hash code accepts counts up to 4096. Corrupt counts can crash this
instrumentation. WS5 leaves it unchanged to preserve the ILP32 reference.

## CI and the unchanged x86 build

`bash tools/ci/compare-x86.sh [base-ref] [head-ref] [new-output-directory]`
runs only on x86_64 Linux with README multilib dependencies and GCC 14 or newer
(`gcc-14` by default, or set `COD2_CC`). GCC 14 is needed for upstream
`-Wno-error=return-mismatch`; see the [GCC 14 porting guide](https://gcc.gnu.org/gcc-14/porting_to.html).
CI installs the versioned GCC 14 multilib packages from Ubuntu. It builds git
archives of base `410342a82da1b2e8286c290274aeb0f1731a6859` and committed HEAD,
with `COD2_X64=OFF`. Uncommitted changes are deliberately excluded.

Both builds use the same gcc, libraries, locale, timezone, fixed source mtimes,
`SOURCE_DATE_EPOCH` from the base commit (`__DATE__`/`__TIME__`), and GCC
`-ffile-prefix-map`/`-fdebug-prefix-map` to one virtual source/build root.
Git discovery is stopped above each archive via `GIT_CEILING_DIRECTORIES`, so
CMake's existing missing-git fallback gives both crash reporters `unknown`.
Passing `-DCOD2_GIT_HASH=...` alone would not work: upstream overwrites it.
No source file is patched, stripped or excluded from comparison. Every game
object, the object path set and both unstripped ELF32 binaries must match;
build IDs are retained. Debug line-number changes from added source guards also
fail this strict check, even when instruction bytes agree. A future difference fails and must be explained, not
silenced by excluding crash_handler or removing arbitrary sections.

This proves equality under the normalized build conditions, not that an old
release artifact from another toolchain has identical bytes. The workflow's
macOS build is an explicitly advisory expected-failure probe until WS1/WS2
land. `macos-latest` is checked for arm64; SDL2 must already be provisioned
(`sdl2-config`). No system packages are installed on the Mac.

Data jobs require a manual dispatch with `game_data=true`, repository variable
`COD2_DATA_RUNNER_ENABLED=true`, secret `COD2_DATA_PATH` (runner-local directory),
and a self-hosted runner labelled `macOS`, `ARM64`, `cod2-data`. Without the
secret the data job is skipped, even if enabled. Use trusted dispatch refs only
on that private runner. The smoke is not a parity claim; cross-architecture
comparison still requires separately recorded reference data. No game-run
artifacts are uploaded by CI.
