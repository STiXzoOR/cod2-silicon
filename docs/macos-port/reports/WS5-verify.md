# WS5 — verification harness

Completed on 2026-10-03 in `~/Projects/cod2-native-wt/verify`, branch
`port/verify`. Read all of `PLAN.md` before implementation. Integration and
publication remain with the orchestrator. No sibling worktrees were accessed;
no remotes, global configuration or system packages were changed. No game
content, vendor executables or decompiler dumps were added.

## Delivered

- `.github/workflows/macos-port.yml`: mandatory synthetic harness checks and
  x86 reference comparison; clearly advisory arm64 build probe; a manually
  enabled, secret-gated, self-hosted Mac game-data smoke job.
- `tools/ci/compare-x86.sh` and `compare-artifacts.py`: fresh git archives of
  upstream `410342a82da1b2e8286c290274aeb0f1731a6859` and committed head, both
  `COD2_X64=OFF`; compare every game object and both unstripped ELF32 binaries.
- `tools/parity/record.py`, `compare.py`, and `docs/macos-port/parity.md`:
  deterministic bounded local-server recording, private manifests/entity
  dumps, first-diverging-frame comparison and prefix/commit bisection guidance.
- `tools/macos/run.sh`, `bench.sh`, `benchmark.py`, and `README.md`: isolated
  writable homepaths, quoted game paths, logging, actual `timedemo <name>`
  command, FPS and millisecond frame-time statistics, competitive cvars and
  Metal HUD interpretation.
- `docs/macos-port/test-server.md`: executable Bash recipe for a separate
  x86 Ubuntu 24.04 host, pinned CoD2x 1.4.6.8/original Linux 1.3 server, LAN
  configuration, ports, Mac/Windows connection and server-side packet capture.
- `docs/macos-port/game-data.md`: user-run SteamCMD instructions for app 2630
  with macOS/Windows overrides, depot map, Mac provenance and IWD layout.
- `tools/tests/test_verification.py`: 14 standard-library tests using synthetic
  files/executables only. Temporary fixtures stay in ignored `output/` and are
  removed; their `.iwd`/`.dm_1` names contain no real game content.

## Verified locally

Run these from this worktree:

```sh
python3 -m unittest discover -s tools/tests -v
# 14 tests, OK: normalized vs raw hash comparisons, first divergence/truncation,
# invalid traces, ELF32/object prerequisites, frame statistics/counts,
# record success/failure/timeout, spaced paths/exit propagation,
# timedemo completion, late completion rejection, truncated-demo rejection,
# incremental split-log reads, and surviving-child process cleanup.

shellcheck tools/ci/*.sh tools/macos/*.sh
# Passed; no findings.

python3 -m compileall -q tools/ci tools/parity tools/macos tools/tests
# Passed.

ruby -e 'require "yaml"; w=YAML.load_file(".github/workflows/macos-port.yml"); abort unless w["jobs"].keys.sort == %w[data-gate game-data-smoke harness macos-arm64 x86-reference]; puts "YAML parsed and job structure verified"'
# Passed. Ruby's YAML 1.1 parser reads unquoted `on` as true; reviewed the
# trigger structure separately. GitHub interprets the `on` trigger normally.

python3 tools/parity/compare.py --help
python3 tools/parity/record.py --help
tools/macos/bench.sh --help
# Passed.

git diff --exit-code 410342a HEAD -- CMakeLists.txt cmake src build third_party
git diff --check port/main..HEAD
# Passed: all existing build/engine inputs are unchanged; whitespace clean.
```

All 12 Bash/sh documentation code blocks also passed `bash -n` via extraction
and stdin validation. A separate temporary compiler probe verified that
`GIT_CEILING_DIRECTORIES` prevents the archived trees inheriting this worktree's
git stamp, and that mapping two roots plus `SOURCE_DATE_EPOCH` gives identical
objects for a fixture containing `__FILE__`, `__DATE__`, `__TIME__` and debug
information. That probe used local Apple clang; it does not replace Linux CI.

Real arm64 configure/build probe:

```sh
cmake -S . -B output/ws5-build-probe -DCOD2_X64=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_C_FLAGS=-ffp-contract=off
# Passed with AppleClang 21.0.0.21000334.
cmake --build output/ws5-build-probe --parallel 3 --target cod2_macos
# Failed as expected: "No rule to make target `cod2_macos'".
```

Logs are local and ignored: `output/ws5-configure.log`, `output/ws5-build.log`.
`bash tools/ci/compare-x86.sh` exits 2 on this Mac with its explicit x86_64 Linux
requirement, before creating builds. The actual Linux reference binaries have
**not** been built or compared here.

`curl -I --fail --max-time 30` returned HTTP 200 for the SteamCMD macOS URL
`https://steamcdn-a.akamaihd.net/client/installer/steamcmd_osx.tar.gz` and the
pinned CoD2x Linux release URL. Neither download was executed. SteamCMD was not
run and no credentials were requested.

## Decisions and limits

Reproducibility uses equal gcc/library versions, locale/timezone, source
mtimes, `SOURCE_DATE_EPOCH`, and `-ffile-prefix-map`/`-fdebug-prefix-map`. Archives
have no git metadata and stop parent discovery, so upstream CMake's existing
`COD2_GIT_HASH="unknown"` fallback is equal in both builds. A CMake cache override
would be overwritten by upstream. Nothing is patched or stripped, including
crash_handler, debug information and build IDs. This is byte equality under
normalized metadata conditions, not equality to an arbitrary release binary.
Debug line movement caused by source guards will also fail the strict check;
keep that in mind when judging an integrated failure.

Selected GCC 14 explicitly: upstream includes `-Wno-error=return-mismatch`,
introduced in GCC 14, while Ubuntu 24.04's default compiler can be older. The
workflow installs versioned GCC 14 multilib packages and sets the i386
pkg-config search path. This follows the README's multilib build rather than
changing shared CMake/source. The Linux package/build steps require execution
on the eventual runner; local validation cannot prove their runtime success.

Statehash is server entity-state parity, not a full simulation/client/renderer
proof. The actual normalized hash omits weapon and animation fields despite
an older comment. It rounds trajectory floats to 1/64, excludes absolute time
and type-specific garbage, and does not hash playerState/script VM/RNG state.
Seed overrides cover the existing two game init/load hooks only. Repeat each
build against itself and match data/config manifests before a cross-build run.
The initial fixture has no player input. A deterministic usercmd/bot fixture
and network compatibility/live-play tests remain future work. Upstream's
1024-element entity array versus 4096 hash count guard is documented, untouched.

The benchmark explicitly sets `cl_freezeDemo 1`: current source gates its CSV
and 50 ms demo stepping there; `timedemo` sets `isTimeDemo` for the FPS summary.
`nextdemo quit` is not consumed by current completion code, so the wrapper stops
its process group after observing completion. Review found and reproduced
false passes for summaries emitted during timeout cleanup and for an engine
truncated-demo diagnostic; both are rejected, with regression coverage. Child
processes surviving their wrapper are killed after the grace period. Polling
reads appended log bytes rather than repeatedly reading all historical output.
Frame times have integer-ms precision; aggregate FPS is rounded. These numbers
cannot prove submillisecond latency, raw mouse input or a stable live 250 fps.

No actual native launch, map load, timedemo, cross-architecture statehash run,
Windows/Mac packet comparison or CoD2x round was possible. SDL relative mode
and zero engine acceleration do not prove macOS acceleration bypass. Metal HUD
visibility over the initial OpenGL path remains unobserved. The SteamCMD archive
may require an unsupported CPU architecture on this no-Rosetta Mac; reachability
is the only bootstrap claim. Depot identities come from the brief; account
entitlements, current manifests and Mac/Windows IWD equality are unverified.

Missing local tools: PyYAML (`python3 -c 'import yaml'` raises
`ModuleNotFoundError`) and actionlint. Used existing Ruby/Psych for YAML syntax
and manually reviewed Actions expressions/dependencies. Installed nothing.

## Orchestrator handoff

All changes are new WS5-owned files. There are no edits to `CMakeLists.txt`,
`src/headers/*`, existing engine sources, data generators or shared build files.
Merge the branch's focused commits together, including tests referenced by CI.
Commits before this report:

| Commit | Scope |
| --- | --- |
| `1d49656` | CI, normalized x86 builds, artifact comparison |
| `3c86bfe` | CoD2x server and licensed-data guides |
| `3af974d` | Statehash recording/comparison and parity guide |
| `351183b` | Mac launch/benchmark wrappers, cvars/HUD guide |
| `baa11c6` | Synthetic harness and cleanup regression tests |

After WS1/WS2 are integrated, ensure `cod2_macos` includes
`src/unix/sysdiff_statehash.c` and retains the frame/seed hooks. Resolve any
platform/runtime failures before trusting the data smoke or benchmark. The
arm64 job is deliberately `continue-on-error: true`; remove that once it is
ready to become a required build gate. It expects preinstalled `cmake` and
`sdl2-config` and checks that `macos-latest` is arm64. Missing runner tools need
provisioning by the runner owner, not package installation by this workstream.

For private data CI, configure `COD2_DATA_RUNNER_ENABLED=true`, secret
`COD2_DATA_PATH`, and a self-hosted Mac with labels `macOS`, `ARM64`, `cod2-data`
and an interactive GUI session. Manually dispatch with `game_data=true` from a
trusted ref. The data job is skipped if the path secret is absent; it uploads
no run artifacts. The path identifies existing licensed data on that runner.

Run `bash tools/ci/compare-x86.sh` on x86 Linux to obtain the missing byte-equality
result, then repeat same-build and cross-build fixture runs in `parity.md`.
Provision the separate x86 Ubuntu server and confirm the Windows client works
before testing WS4's native handshake. Keep private data, configs, logs and
captures outside commits.

Status: WS5 tooling/docs complete, locally verified within available hardware;
project runtime/build acceptance remains with the orchestrator. Execution was
native/in-session with a bounded docs worker and three read-only review workers;
no external implementation engine, detached run or shipping workflow was used.
Standalone shipping skipped; no push, PR or issue.
