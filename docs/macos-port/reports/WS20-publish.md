# WS20 — public release documentation, provenance and CI

Date: 2026-10-04. Branch: `port/publish`. Worktree:
`~/Projects/cod2-native-wt/publish`. Starting commit: `df08d50` (integrated WS18).

## Result

The publication deliverables are written and committed: CoD2 Silicon player
README, MIT scope, evidence-backed credits, contribution/security/conduct
policies, issue/PR templates, changelog, roadmap, development-log index,
privacy cleanup and replacement CI. This workstream changed only documentation
and `.github/` metadata. No `src/`, `cmake/`, `tools/`, `scripts/`, `tests/`,
shared headers or top-level CMake files changed. The original i386 compilation
inputs remain untouched. No sibling worktree was read or written, no game or
real benchmark was launched, no packages were installed, and no remotes,
GitHub repo settings, issues, PRs, tags or pushes were changed.

**Public release is not yet validated end to end.** Stock and CoD2x arm64
configuration still requires private reference binaries, and an inherited
full CoD2x fixture is stale. These are outside WS20's writable source scope.
The workflow makes failures visible; there is no expected-failure probe or
`continue-on-error`. Resolve the prerequisites and nine WS21 markers below
before publishing. No GitHub Actions run, release asset, installation on a
second Mac or macOS 13 run is claimed here.

## What was verified locally

Commands ran in this worktree. Compiler-bearing configuration and fixture
commands used `taskpolicy -b nice -n 19`, as requested. Pure Python harness
reruns also used `nice -n 19`; they launch synthetic scripts, not the game.

| Command / check | Result |
| --- | --- |
| `ruby -r yaml -e 'ARGV.each { \|p\| YAML.parse_file(p) }' .github/workflows/ci.yml .github/ISSUE_TEMPLATE/*.yml` | All four YAML files parse with installed Ruby 4.0.7 / Psych. |
| Extract all 19 CI `run` scripts with Ruby YAML and pass each to `bash -n` | All pass shell syntax validation. |
| `shellcheck tools/ci/*.sh tools/macos/*.sh` | Pass; the preserved shellcheck scope remains unchanged. |
| `python3 -m unittest discover -s tools/datagen -p test_datagen.py -v` | 12/12 pass; synthetic grammar/recovery fixtures, no original binary. |
| `python3 tools/abi/test_audit.py` | 4/4 pass, using independent synthetic Clang translation units. |
| `python3 tools/abi/test_imports.py` | 6/6 pass; synthetic import/load-depth cases. |
| `python3 tests/rendering/shader_setup.py` | 3/3 pass; synthetic cache/manifest rejection tests. |
| `python3 -m unittest discover -s tests/lp64/renderer -p test_wine_draw_trace.py -v` | 1/1 pass; synthetic trace parser. |
| `sh tests/cod2x/run_native.sh` | Pass: URL, mouse, input, native AppleEvent queue, synthetic app/plist/signature and watchdog/crash fixtures. |
| Compare upstream README bytes after the single provenance header | Exactly equal to `git show df08d50:README.md`. The upstream README body was not edited. |
| Compare `LICENSE` with standard SPDX MIT text after substituting only the requested copyright | Exact equality. DCO is the canonical site's verbatim `<pre>` text; Covenant 2.1 fills its contact placeholder and normalizes the terminal newline. |
| Compare old/new x86 workflow job text | Entire `x86-reference` job remains byte-for-byte identical. |
| Check local Markdown links in new docs | All resolve after this report was added. External release/installer existence remains a WS21/orchestrator check. |
| `git diff df08d50 HEAD -- src cmake tools scripts tests CMakeLists.txt` | Empty. No implementation/build inputs changed. |
| `git diff --check df08d50` | Pass after removing a trailing blank line from the Code of Conduct in the final branch-wide check. |

PyYAML is unavailable (`python3 -c 'import yaml'` raises
`ModuleNotFoundError`); `actionlint` is not installed. No tool was installed.
Ruby/Psych supplies local YAML syntax validation, not a complete Actions
schema/expression lint. Commands were checked against the scripts themselves.
The runner label `macos-15` is listed as a standard Apple silicon/M1 runner
in [GitHub's runner documentation](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
The workflow also asserts `uname -m = arm64`. It provisions SDL only on the
future ephemeral Actions runner; no Homebrew install ran on this Mac.

### Failing and unavailable checks

1. **No private-input configure:**

   ```sh
   taskpolicy -b nice -n 19 cmake -S . -B output/ws20/no-private-inputs \
     -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
     -DCOD2_STABS_BINARY=/nonexistent/cod2-mac-stabs \
     -DCOD2_VALUES_BINARY=/nonexistent/cod2-mac-steam
   ```

   Exits 1 at `cmake/datagen.cmake:16`: typed generation needs the STABS
   binary. That module separately requires the Steam scalar-value binary
   too. Current public hosted CI has neither, so both real Release build
   steps will fail until the source-build prerequisite is resolved. WS20
   cannot fix this by editing CMake/tools, committing generated payloads or
   fetching proprietary inputs. `scripts/install.sh` is absent in this
   checkout; its documented interface is the user's WS21 contract, not a
   locally verified installer.

2. **Inherited `tests/cod2x/run_full.sh`:** production protocol, identity,
   policy, feature, mouse, dvar-pool, big-info, radar, pose, demo, playback
   and input checks pass, then the URL probe aborts at
   `test_url_native.m:34` (`strstr(arguments, expectedGame)`). The script
   creates only an empty `game/main`; the current first-run validator needs
   `iw_00.iwd` through `iw_15.iwd` filenames. `run_native.sh` already creates
   those synthetic placeholders and passes. The orchestrator should update
   the stale full-suite fixture similarly and account for WS21's final app
   naming/setup. The source/test file was not modified. CI bounds the full
   suite to five minutes in case the folder picker waits for input on a
   clean runner, and still runs the independent native/identity steps.

3. **Harness intermittency:** both complete
   `python3 -m unittest discover -s tools/tests -v` runs finish **13/14**.
   `test_benchmark_completion_and_shutdown` reports `[Errno 1] Operation not
   permitted`; it passes in an isolated rerun:

   ```sh
   nice -n 19 env PYTHONPATH=tools/tests python3 -m unittest \
     test_verification.VerificationTests.test_benchmark_completion_and_shutdown -v
   ```

   The full-suite failure occurs with `taskpolicy -b` and with `nice` alone,
   so it cannot be attributed to background policy from this evidence.
   It is a synthetic fake-engine test, not a real benchmark. No tools/tests
   source was changed; the CI harness continues to run the whole suite and
   must be checked on Linux by the orchestrator. Do not report a full local
   harness pass.

4. **Not run here:** full engine builds, binary-dependent ABI/import checks,
   Linux ILP32 artifact comparison, interactive platform/display/audio suites,
   real CGL rendering suites, licensed-data smoke/parity, installations and
   performance measurements. WS19 owns the active performance experiment;
   WS20 did not disturb the game/display. CI includes the data-free suites;
   inclusion is not execution evidence.

## Documentation decisions and merge notes

- The README uses the agreed name, app, release archive, signing/Gatekeeper
  instructions and source installer interface. Bundle identity is
  `io.github.stixzoor.cod2silicon`, release `0.1.0` (WS21 implementation).
- CoD2x inventory counts were calculated directly from WS10's parity table:
  **68 = 41 Done + 12 Native-equivalent + 2 Partial + 13 Skipped**. The
  README explicitly calls this implementation coverage, not live acceptance.
- Rendering numbers use WS15's final merged-tree table (2.0050 / 1.8726 /
  0.5856 MAE), not superseded early figures. WS18 supersedes the firing
  crash in that older rendering report and verifies the combat soak.
- Online wording preserves WS14's short, mostly empty-server sessions and
  does not claim crowded-server desync acceptance. Performance uses all six
  WS18 live averages/1% lows, preserves backing-versus-physical-display
  qualifications and does not claim constant 333 or active Game Mode.
- The data guide was updated from early pre-bring-up assumptions: owned
  Windows IWDs work in verified native sessions; Steam Mac shaders are
  extracted locally; Steam's partial STABS is not the full type reference.
- MIT text is unmodified except the requested copyright. NOTICE limits it to
  this project's original work after `410342a`, preserving upstream and
  third-party rights. No licensing outreach occurred.
- Contributor commands preserve fixture assertions: the actual Release
  compile database is backed up before replacing `-DNDEBUG` with `-UNDEBUG`
  in test-only command inputs. The full private ABI command uses the engine
  database. Platform CTest uses Debug so its assertions are enabled. No
  production build flags or test sources were changed.
- CI retains the x86 job and Python/shellcheck harness, builds both Release
  clients, runs all current aggregate data-free runtime suites and performs
  function/callback ABI audits. The full import audit and layout/datagen
  comparisons consume private Mac binaries and remain local gates. Per-change
  source guard tools are documented alongside the authoritative CI x86
  object/binary comparison; historical diagnostic replay tools are not
  represented as acceptance tests.
- Independent CI steps continue after a failure, and the renderer/FX block
  aggregates exit status so one failed fixture does not skip every later
  suite. No private-data runner, game-data secrets or raw game artifacts are
  added to public CI. `.github/workflows/release.yml` was not touched.
- Enable **GitHub private vulnerability reporting** before publication;
  SECURITY and issue-template links rely on that repository setting. Conduct
  contact uses the maintainer's GitHub profile, as requested. No repository
  settings were changed by this workstream.
- Preserve `docs/macos-port/` paths. Its index labels these reports as a
  historical AI-orchestrated log; WS19/WS21 are named but not linked to
  nonexistent reports. Add their index links when their reports merge.

### Focused commits

- `a6646eb` — Document MIT scope and verified project credits.
- `be4dea0` — Introduce CoD2 Silicon player docs and release roadmap.
- `960f195` — Define contribution, conduct and security reporting policies.
- `2899e5a` — Remove personal machine details from port reports.
- `7318455` — Run Apple silicon Release builds and data-free CI suites.
- `7fb608b` — Credit YAML validation and macOS system tools.
- `9f99285` — Link the preserved WebAssembly build guide.
- `7408b03` — Put player setup before detailed verification results.
- `4dfa8cd` — Record WS20 publication evidence and release handoff.

The final whitespace correction and report update are committed separately.
Every WS20 commit has a DCO sign-off.
Merge the full `port/publish` branch after reconciling WS21's installer/app
contract. The only likely overlap is documentation; no source/CMake/shared
headers were changed. Re-run the public pipeline after WS21 integration and
retain the private/live merge gates before a release.

## File inventory

Status is relative to `df08d50`. All report changes other than this new report
are privacy-only replacements; technical facts and command meaning remain.

| Status | File |
| --- | --- |
| Created | `.github/ISSUE_TEMPLATE/bug_report.yml` |
| Created | `.github/ISSUE_TEMPLATE/config.yml` |
| Created | `.github/ISSUE_TEMPLATE/feature_request.yml` |
| Created | `.github/PULL_REQUEST_TEMPLATE.md` |
| Created | `.github/workflows/ci.yml` |
| Removed | `.github/workflows/macos-port.yml` |
| Created | `CHANGELOG.md` |
| Created | `CODE_OF_CONDUCT.md` |
| Created | `CONTRIBUTING.md` |
| Created | `CREDITS.md` |
| Created | `DCO.txt` |
| Created | `LICENSE` |
| Created | `NOTICE.md` |
| Changed | `README.md` |
| Created | `SECURITY.md` |
| Created | `docs/ROADMAP.md` |
| Created | `docs/macos-port/README.md` |
| Changed | `docs/macos-port/game-data.md` |
| Changed | `docs/macos-port/reports/WS1-macos-build.md` |
| Changed | `docs/macos-port/reports/WS10-cod2x-full.md` |
| Changed | `docs/macos-port/reports/WS11-bringup.md` |
| Changed | `docs/macos-port/reports/WS12-abi-audit.md` |
| Changed | `docs/macos-port/reports/WS13-perf.md` |
| Changed | `docs/macos-port/reports/WS14-online.md` |
| Changed | `docs/macos-port/reports/WS15-render.md` |
| Changed | `docs/macos-port/reports/WS16-vm-safety.md` |
| Changed | `docs/macos-port/reports/WS17-fullscreen.md` |
| Changed | `docs/macos-port/reports/WS18-ship.md` |
| Changed | `docs/macos-port/reports/WS2-datagen.md` |
| Changed | `docs/macos-port/reports/WS3-platform.md` |
| Changed | `docs/macos-port/reports/WS4-cod2x.md` |
| Changed | `docs/macos-port/reports/WS5-verify.md` |
| Changed | `docs/macos-port/reports/WS6-renderer.md` |
| Changed | `docs/macos-port/reports/WS6-script.md` |
| Created | `docs/upstream-opencod2-README.md` |
| Created | `docs/macos-port/reports/WS20-publish.md` |

## Privacy and repository hygiene

A whole tracked-text-tree scan checked the developer-name/Steam-name patterns,
populated `codkey` assignments, dashed and quoted uppercase CD-key shapes,
long quoted credential literals and common GitHub/OpenAI token prefixes.
It printed only filenames, line numbers and counts; no real secret was printed.

- Replaced **20 absolute home-path lines in 16 reports** with `~/…`.
- Replaced the WS18 list of personal apps with **“other user applications”**,
  retaining the unowned profiler/background-load qualification.
- Final scan before this report: **zero personal absolute-home matches**, no
  supplied Steam-name matches, no dashed/quoted uppercase key candidates,
  no populated quoted credential literals and no token-pattern matches.
- The only `codkey` assignment-like hit is the empty field-name marker in
  PLAN.md; no value is present. It is retained as technical documentation.
- The remaining case-insensitive developer-name hits are **8 lines in 5
  files**, all the explicitly chosen public GitHub owner/bundle identity.
  Those are required project metadata, not a Steam login or local pathname.
- A broader credential-assignment pattern found 64 lines in 22 existing
  source/diagnostic files. Review identified identifier/function expressions
  such as parser token fields and password-dvar registrations, plus diagnostic
  echoes, rather than literal credentials; the literal-value follow-up scan
  found none. No source changes were made for these false positives.
- No committed asset/binary/shader/demo/key/dump was added. Prior Git history
  was not rewritten; the report cleanup affects the current public tree,
  not historical commits.

The upstream README body was preserved byte-for-byte after its provenance
header. Personal cleanup was applied to committed docs, not sibling trees or
external references. LICENSE's requested real-name copyright and DCO sign-off
identity remain intentionally public.

## Credits evidence table

Read PLAN.md, indexed/scanned every existing report and all three external
`research/*.md` reports, then checked all **20 reference Git clones** with
`git -C <repo> remote -v` and looked for license/COPYING files. Public metadata
and, where necessary, actual license endpoint content were checked via
`gh api repos/<owner>/<repo>` and `gh api repos/<owner>/<repo>/license`.
Homebrew formula metadata was read, not installed. The credits distinguish
actual port dependencies from surveyed/proposed references and downloaded but
unused baseline components.

The local Mac reference was independently compared to the i386 fat-binary
slice in CoD2x: byte equality, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
No binary was copied or committed. Research identifies the earlier Mac 1.0
blob lineage and the separate 2013 Steam binary; both distinctions are recorded.

Notable corrections to old notes: both CoD2rev clones have AGPL-3.0 LICENSE;
xtnded/cod2 has GPL-2.0 COPYING; current GitHub metadata declares MIT for
codremaster, GPL-3.0 for CoD4-DM1, GPL-2.0 for iw2clientdll and Apache-2.0
for rutkowski-tomasz/cod2-docker. DXMT's actual license is LGPL-2.1-or-later,
not an inference from its hosting service's `NOASSERTION` value. Tool/license
endpoint texts were read for curl, zlib, Speex, LLVM, Python, Git, Wine,
MinGW, Emscripten, Swift, DepotDownloader and Keep a Changelog.

Paths below are under `~/Projects/cod2-native-refs/` when they start with
`research`, `CoD2x`, `CoD2rev_Server` or `xtnded-cod2`. Report identifiers
refer to this repository's `docs/macos-port/reports/`.

| Credit entry | Verification evidence |
| --- | --- |
| [opencod2](https://github.com/opencod2/opencod2) | Upstream remote; GitHub repository metadata; original README; research/mac-client-reconstructions.md |
| [CoD2x](https://github.com/callofduty2x/CoD2x) | CoD2x remote; no top-level license; cod2x-compat.md; WS4/WS8/WS10 |
| [cod2engine](https://github.com/nawaftahir/cod2engine) | research-clones/cod2engine remote and license search; research/engine-server-reconstructions.md; WS6/WS9/WS16 |
| [CoD2rev_Server (upstream)](https://github.com/voron00/CoD2rev_Server) | research-clones/CoD2rev_Server-voron00 remote and LICENSE; research/engine-server-reconstructions.md |
| [CoD2rev_Server (CoD2x fork)](https://github.com/callofduty2x/CoD2rev_Server) | CoD2rev_Server remote and LICENSE; PLAN.md |
| [KisakCOD](https://github.com/SwagSoftware/KisakCOD) | research-clones/KisakCOD remote and LICENSE; PLAN.md; WS15/WS16 |
| [xtnded/cod2](https://github.com/xtnded/cod2) | xtnded-cod2 remote and COPYING.txt; research/mac-client-reconstructions.md |
| [cod2-mp-macho](https://github.com/yctn/cod2-mp-macho) | research-clones/cod2-mp-macho remote and license search; research/mac-client-reconstructions.md |
| [cod2_dasm_out](https://github.com/yctn/cod2_dasm_out) | research-clones/cod2_dasm_out remote and license search; research/mac-client-reconstructions.md |
| [Original Call of Duty 2 / Mac port](https://store.steampowered.com/app/2630/Call_of_Duty_2/) | PLAN.md; research/mac-client-reconstructions.md binary table; WS2/WS15/WS18 |
| [cod2map](https://github.com/7894752/cod2map) | research-clones/cod2map remote and license search; research/tools-ecosystem.md |
| [cod2map arm64 fork](https://github.com/riicchhaarrd/cod2map) | research-clones/cod2map-riicchhaarrd remote and license search; research/tools-ecosystem.md |
| [CoD2Unity](https://github.com/Hommsin/CoD2Unity) | research-clones/Hommsin_CoD2Unity remote and LICENSE; research/tools-ecosystem.md |
| [cod2unity fork](https://github.com/BUZDOLAPCI/cod2unity) | research-clones/BUZDOLAPCI_cod2unity remote and license search; research/tools-ecosystem.md |
| [PyD3DBSP](https://github.com/mauserzjeh/PyD3DBSP) | research-clones/mauserzjeh_PyD3DBSP remote and LICENSE; research/tools-ecosystem.md |
| [cod-asset-importer](https://github.com/mauserzjeh/cod-asset-importer) | research-clones/mauserzjeh_cod-asset-importer remote and LICENSE; research/tools-ecosystem.md |
| [iwi2dds (Go)](https://github.com/mauserzjeh/iwi2dds) | research-clones/mauserzjeh_iwi2dds remote and LICENSE; research/tools-ecosystem.md |
| [d3dbsp](https://github.com/riicchhaarrd/d3dbsp) | research-clones/riicchhaarrd_d3dbsp remote and LICENSE; research/tools-ecosystem.md |
| [material_util](https://github.com/riicchhaarrd/material_util) | research-clones/riicchhaarrd_material_util remote and LICENSE; research/tools-ecosystem.md |
| [iwi2dds (Python)](https://github.com/riicchhaarrd/iwi2dds) | research-clones/riicchhaarrd_iwi2dds remote and license search; research/tools-ecosystem.md |
| [demoparser](https://github.com/KILLTUBE/demoparser) | research-clones/KILLTUBE_demoparser remote and license search; research/tools-ecosystem.md |
| [cod2_master_serv](https://github.com/voron00/cod2_master_serv) | research-clones/voron00_cod2_master_serv remote and license search; research/tools-ecosystem.md |
| [zk_libcod](https://github.com/ibuddieat/zk_libcod) | research/tools-ecosystem.md; GitHub metadata LICENSE.md |
| [libcod (M-itch)](https://github.com/M-itch/libcod) | research/tools-ecosystem.md; GitHub metadata |
| [libcod (voron00)](https://github.com/voron00/libcod) | research/tools-ecosystem.md; GitHub metadata |
| [CoD2 Docker server](https://github.com/bgauduch/call-of-duty-2-docker-server) | research/tools-ecosystem.md; GitHub metadata |
| [cod2-docker](https://github.com/rutkowski-tomasz/cod2-docker) | research/tools-ecosystem.md; GitHub metadata |
| [Greyhound](https://github.com/Scobalula/Greyhound) | research/tools-ecosystem.md; GitHub metadata |
| [cod_masterserver](https://github.com/yorick1989/cod_masterserver) | research/tools-ecosystem.md; GitHub metadata |
| [codremaster](https://github.com/schwarz/codremaster) | research/tools-ecosystem.md; GitHub metadata (corrects old no-license note) |
| [CoD4-DM1](https://github.com/Iswenzz/CoD4-DM1) | research/tools-ecosystem.md; GitHub metadata (corrects old no-license note) |
| [zPAM](https://github.com/eyza-cod2/zpam3) | research/tools-ecosystem.md; WS14/WS15; GitHub metadata |
| [iw2clientdll](https://github.com/xtnded/iw2clientdll) | research/tools-ecosystem.md; GitHub metadata (corrects old no-license note) |
| [codscriptdoc](https://github.com/M-itch/codscriptdoc) | research/tools-ecosystem.md; GitHub metadata |
| [Killtube](https://killtube.org/) | research/tools-ecosystem.md; research/mac-client-reconstructions.md |
| [SDL3](https://github.com/libsdl-org/SDL) | WS3; brew info --json=v2 sdl3; installed license; cmake/macos-arm64.cmake |
| [sdl2-compat](https://github.com/libsdl-org/sdl2-compat) | WS3/WS17/WS18; brew info --json=v2 sdl2-compat; installed LICENSE.txt |
| [libcurl / curl](https://curl.se/) | cmake/macos-arm64.cmake; WS10/WS14; curl/curl license endpoint |
| [zlib](https://zlib.net/) | cmake/macos-arm64.cmake; src/PC/zlib/zlib.h; madler/zlib license endpoint |
| [Speex](https://www.speex.org/) | src/PC/speex/bits.c and other headers; xiph/speex COPYING; WS12 |
| [libjpeg-turbo / Independent JPEG Group](https://github.com/libjpeg-turbo/libjpeg-turbo) | third_party/libjpeg-turbo/LICENSE.md and README.ijg; WS11/WS12; src/platform/macos_jpeg.c |
| [macOS SDK frameworks](https://developer.apple.com/documentation/) | cmake/macos-arm64.cmake; tests/platform/CMakeLists.txt; WS3/WS17/WS18 |
| [CMake](https://cmake.org/) | PLAN.md; WS1/WS5/WS18; Kitware/CMake metadata |
| [Apple clang / Xcode Command Line Tools](https://developer.apple.com/xcode/) | PLAN.md; WS1/WS2/WS12; llvm/llvm-project LICENSE.TXT |
| [LLDB](https://lldb.llvm.org/) | WS14/WS16; llvm/llvm-project LICENSE.TXT |
| [Instruments / xctrace / Metal performance tools](https://developer.apple.com/instruments/) | WS13 Time Profiler commands; WS8 metalperftrace |
| [Swift](https://www.swift.org/) | WS15/WS18; tests/rendering/*.swift; tools/cod2x/app_icon.swift; Swift metadata |
| [Python](https://www.python.org/) | WS2/WS5/WS12/WS18; python/cpython LICENSE |
| [Git](https://git-scm.com/) | All reports; git/git COPYING |
| [GitHub CLI](https://cli.github.com/) | research/*.md; WS20 gh api evidence; cli/cli metadata |
| [GitHub Actions / checkout / upload-artifact](https://github.com/features/actions) | WS5; .github/workflows/ci.yml; action repository metadata |
| [Homebrew](https://brew.sh/) | PLAN.md; WS3; brew formula metadata; Homebrew/brew metadata |
| [ShellCheck](https://www.shellcheck.net/) | WS5; CI harness; koalaman/shellcheck metadata |
| [GCC / GNU binutils](https://gcc.gnu.org/) | tools/ci/compare-x86.sh; parity.md; research/engine-server-reconstructions.md |
| [MinGW-w64](https://www.mingw-w64.org/) | Original README; cmake/toolchain-mingw32.cmake |
| [MSVC / Visual Studio](https://visualstudio.microsoft.com/) | Original README; CMakePresets.json |
| [Emscripten](https://emscripten.org/) | src/web/README.md; original CMake WASM target |
| [DepotDownloader](https://github.com/SteamRE/DepotDownloader) | game-data.md; WS5; SteamRE/DepotDownloader LICENSE |
| [Steam / SteamCMD](https://developer.valvesoftware.com/wiki/SteamCMD) | WS5/WS8; game-data.md |
| [Wine](https://www.winehq.org/) | WS8/WS15; wine-mirror/wine; Sikarugir wine LICENSE |
| [Highball](https://github.com/gauthierpiarrette/highball) | WS8 download and renderer links; GitHub metadata |
| [Sikarugir](https://github.com/Sikarugir-App/Sikarugir) | WS8; Sikarugir/Sikarugir metadata; Sikarugir/wine LICENSE |
| [Gcenx macOS Wine builds](https://github.com/Gcenx/macOS_Wine_builds) | WS8; GitHub metadata |
| [DXVK / D9VK macOS / Kegworks builds](https://github.com/Gcenx/DXVK-macOS) | WS8/WS15; doitsujin/dxvk and Gcenx/DXVK-macOS metadata |
| [MoltenVK](https://github.com/KhronosGroup/MoltenVK) | WS8; KhronosGroup/MoltenVK metadata |
| [Rosetta 2](https://support.apple.com/en-us/102527) | PLAN.md; WS8 |
| [DXMT](https://github.com/3Shain/dxmt) | WS8 runtime inventory; GitHub metadata |
| [D3DMetal / Game Porting Toolkit](https://developer.apple.com/games/game-porting-toolkit/) | WS8 explicit exclusion |
| [Ruby / Psych](https://github.com/ruby/psych) | WS20 ruby -r psych gem specification; Ruby official license.txt |
| [macOS signing and system utilities](https://developer.apple.com/documentation/security) | WS8/WS18 signing/plist commands; WS20 taskpolicy; macOS SDK/system tools |
| Codex CLI / gpt-6.1-sol | User's workstream brief; openai/codex metadata and Apache-2.0 license. |
| Claude Code | User's workstream brief; anthropics/claude-code metadata (no repository license) and product terms. |
| Contributor Covenant 2.1 | Canonical version 2.1 Markdown downloaded from contributor-covenant.org; contact placeholder filled, attribution preserved. |
| DCO 1.1 | Canonical developercertificate.org verbatim text. |
| Keep a Changelog | Canonical 1.1.0 format; olivierlacan/keep-a-changelog MIT license endpoint. |

## WS21 confirmation markers

Exactly **nine** markers exist outside this report. Resolve/remove each after
checking the merged implementation. No elapsed time or assumed sibling work
counts as verification.

| Location | Confirmation |
| --- | --- |
| README.md requirements | Minimum supported macOS: currently `macOS 13 or later`. |
| README.md source install | `scripts/install.sh`, CLT/CMake guidance and clean-checkout build prerequisites. |
| README.md first run | Folder discovery/picker, CD-key prompt, preference file and renamed support path. |
| README.md development build | Public source build versus current private-reference prerequisite. |
| CONTRIBUTING.md build setup | Clean-checkout source build and CI datagen inputs. |
| docs/macos-port/game-data.md shaders | Discovery paths and extraction runtime dependency on another Mac. |
| docs/macos-port/game-data.md STABS | Installer resolution of the full-STABS type-source prerequisite. |
| docs/macos-port/game-data.md first run | Picker, CD-key prompt and `CoD2 Silicon` support directory. |
| .github/workflows/ci.yml datagen comment | Public hosted clean-checkout stock and CoD2x builds. |

Also verify the agreed asset names `CoD2-Silicon-<version>-macos-arm64.zip`
and `SHA256SUMS`, ad-hoc signature, bundle ID, quarantine instructions, and
explicit-app `cod2x://` routing on a second Mac. These are contract terms in
WS20, not implementation proof. Minimum OS and packaged runtime dependencies
must be decided by WS21; no hardcoded development-only path should be needed
on a player's machine.

## Ready-to-create roadmap issues

These are drafts only. No issue was created. Paths/commands in the bodies are
reproduction/acceptance suggestions for the later workstream, not benchmarks
performed by WS20.

### 1. Hold 333 fps with stable fullscreen frame pacing

**Body:** WS18 approaches 333 fps on average, but capped live 1% lows are
205.2–233.0 fps. Swap/presentation and Cocoa event waits dominate measured
hitches. Integrate WS19's measured diagnosis and changes; retain raw mouse,
packet cadence and exact render backing. Validate one foreground client with
original shaders and a quiet desktop at 1080p, then repeat other supported
render sizes. Publish average, 1% low, P99 and worst-frame phase evidence from
`tools/macos/live-bench.py` and matched timedemos. Do not equate the 3 ms
physics quantum with presentation stability. Acceptance: an agreed sustained
333-fps criterion and reproducible evidence, unchanged competitive caps and
zero ABI/i386 regressions.

### 2. Verify and enable macOS Game Mode in native fullscreen

**Body:** WS17/WS18 establish plist eligibility and paused/available states,
not active ON. Finish Cocoa fullscreen/Space transitions and foreground input
focus without breaking exact backing or exclusive/desktop fallback. Verify
Game Mode in both menu-bar UI and system logs, then run matched isolated
ON/OFF captures. Acceptance: real ON state for the shipped bundle, reliable
keyboard/raw mouse focus, and correct display restoration after Cmd-Tab,
minimize and quit. Publish measured benefit or lack of benefit.

### 3. Finish the standalone arm64 dedicated-server link

**Body:** `cod2_macos_ded` compiles but still has renderer/data roots that
prevent a standalone native link. The current workaround is the client with
`+set dedicated 1`. Define the headless dependency boundary, resolve globals
with real types/ownership and remove client-only dependencies under native
guards. Acceptance: `cmake --build build-macos --target cod2_macos_ded`
succeeds, starts a local licensed-data map, accepts a stock/CoD2x-compatible
client as appropriate, shuts down cleanly and retains unchanged i386 output.
Do not claim a full CoD2x server implementation from the client feature gate.

### 4. Replace the 185 unresolved placeholder globals with verified storage

**Body:** The ABI import inventory records 185 zero-filled placeholders.
Classify each by actual type, size, initializer, ownership and active use,
using STABS/disassembly and source evidence. Prioritize ASan-observed bounds
problems and reachable runtime state. Replace placeholders in small guarded
steps; do not enlarge arbitrary arrays or infer correctness from a successful
link. Acceptance: a tracked inventory that shrinks with verified fixes,
production-function sanitizer tests, zero ABI mismatches and byte-identical
legacy artifacts. Keep all proprietary evidence outside git.

### 5. Implement native microphone capture for multiplayer voice

**Body:** Native audio output exists, but capture is unavailable. Implement
an explicit macOS microphone permission flow, device selection/recovery and
capture-to-compatible codec path, preserving server protocol and mute
semantics. Avoid work or blocking permission/UI calls in the frame-critical
path. Acceptance: real capture/playback interoperability with a trusted test
server/client, device-change and permission-denied tests, cleanup on quit and
no regression in input/frame pacing. Keep voice recordings private.

### 6. Restore intro cinematic playback from owned content

**Body:** The app currently skips intro cinematics. Identify the native
format/decoder and playback lifecycle from verified owned/reference inputs;
choose a maintainable decoder or platform adapter with clear licensing.
Acceptance: correct video/audio timing, user skip behavior, missing/corrupt
file handling and clean return to the menu without GL/audio state leakage.
Provide content-free fixtures and keep real cinematic assets outside git.

### 7. Improve setup, diagnostics and player quality of life

**Body:** Make game-folder recovery, input/display options and server-browser
use clearer, with errors that tell the player what to do next. Build on WS21's
portable installer and first-run contract. Select improvements from concrete
player reports; preserve archived configs, protocol negotiation and
competitive restrictions. Acceptance: documented player flows, safe migration
of existing preferences, accessible controls and no accidental exposure of
CD keys/HWIDs in diagnostic exports. Keep internal implementation details out
of player prompts unless they help a decision.

### 8. Explore optional modern post-processing with a faithful classic mode

**Body:** Research “2026 era” post-processing as an optional mode while
preserving the original shader appearance and server policy. Begin with
matched camera/reference captures and measured GPU/CPU cost, not a broad
renderer rewrite. Acceptance for a prototype: reversible settings, unchanged
classic rendering comparisons, quantified frame-time/input-latency cost and
compatible HUD/FX. Decide which effects to ship only after evidence; document
remaining bloom/channel-mixing/shadow parity separately.

### 9. Prototype a Metal renderer against the existing renderer contracts

**Body:** Evaluate a possible native Metal renderer after documenting the
current material, geometry, texture, shader-constant and draw-command
contracts. Implement a narrow prototype outside the default rendering path,
with original work and clearly licensed dependencies. Acceptance: a small
stock scene rendered from player-owned data, matched screenshot/draw evidence,
measured cost and a written feasibility decision. Preserve the OpenGL classic
path and i386 output; a prototype is not a release commitment.

## Proposed GitHub metadata

**Description (user's tagline, under 350 characters):**

> Call of Duty 2 multiplayer, native on Apple silicon. CoD2x 1.4 compatible, built for 333 fps.

**15 topics:** `call-of-duty-2`, `cod2`, `cod2x`, `macos`, `apple-silicon`,
`arm64`, `game-engine`, `reverse-engineering`, `opengl`, `multiplayer`,
`native`, `sdl3`, `c`, `cpp`, `game-preservation`.

No repository metadata was changed. The orchestrator owns publication,
release tagging/assets, private-reporting configuration and issue creation.
