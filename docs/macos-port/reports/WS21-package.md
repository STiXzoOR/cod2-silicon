# WS21 — portable unsigned CoD2 Silicon packaging

The release bundle is self-contained, ad-hoc signed, and targets **macOS 13.0, arm64**. Native first-run shader extraction replaces runtime Python. The public source-build contract has a remaining blocker: typed-data generation still needs the separately licensed STABS reference. A fresh clone builds and installs successfully when that input is supplied; a stranger with only Windows game data cannot yet build from source.

Worktree: `~/Projects/cod2-native-wt/package`, branch `port/package`, base `df08d50`. Tested on the PLAN host (Apple M6, macOS 27.0.1, Xcode 27). Implementation ran inline, without delegated agents. The orchestrator owns review, merge, publication and hosted workflow execution. No pushes, PRs, issues, remote changes, system package installations, or sibling-worktree file reads/writes. The required fresh clone lives under this worktree's ignored `output/ws21/` directory; its automatically created origin points at this worktree. All compile-heavy commands ran under `taskpolicy -b nice -n 19`, including compilation invoked by test drivers. Game launches check `pgrep -fl cod2_macos`, inspect process names to exclude compilers, and wait in 60-second intervals for WS19. Only owned test processes were stopped.

## Identity and deployment

- Bundle: `CoD2 Silicon.app`; name/display name: `CoD2 Silicon`.
- ID: `io.github.stixzoor.cod2silicon`; short version: `0.1.0`.
- URL scheme: `cod2x`.
- User data: `~/Library/Application Support/CoD2 Silicon`.
- The server-facing CoD2x identity/version remains **1.4.6.8**. No protocol/identity implementation files changed; the existing CoD2x regression tests pass.

`cmake/macos-arm64.cmake` sets the native deployment target to 13.0, and promotes `unguarded-availability` and `unguarded-availability-new` to errors on platform sources. Both SDL builds explicitly set arm64 and 13.0. `vtool -show-build` reports `minos 13.0` for all three actual Mach-O files in the release bundle, and for both stock/CoD2x client executables. SDK is 27.0. This is a verified build/deployment minimum; actual execution on macOS 13, 14 or 15 hardware was unavailable.

Development builds may still link Homebrew SDL, whose current host binary has `minos 27.0` and produces a linker warning against the engine's 13.0 target. The release/source-install scripts build their own SDL at 13.0 and have no such dependency. The CMake app target writes `${CMAKE_BINARY_DIR}/CoD2 Silicon.app` by default; `COD2_MACOS_APP` can override the destination. A release CMake app target requires `COD2_MACOS_SDL_PREFIX` and passes it to the bundler. It never overwrites `~/Applications/CoD2x Native.app`.

## SDL and the bundle

`scripts/build-sdl.sh` downloads these official source tarballs from [SDL's release directory](https://www.libsdl.org/release/), checks their pinned SHA-256 before extraction, and caches downloads/builds/install files under the selected build directory:

| Component | Version | Tarball SHA-256 |
| --- | --- | --- |
| SDL3 | 3.4.10 | `12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785` |
| sdl2-compat | 2.32.72 | `a14d2f78dad8e83ef1039b6534ace4d14f11f5b11d023af989affd70ac1bb35e` |

Both use CMake, Release optimization, shared dylibs and the macOS SDK. SDL3 tests/static libraries/framework packaging are disabled. sdl2-compat uses the pinned SDL3 headers. No Homebrew installation is needed; Homebrew/local prefixes are excluded from release dependency discovery. zlib and curl come from the SDK/system libraries.

`make_macos_app.py --frameworks <install-prefix>` copies SDL dylibs and their relative symlinks into `Contents/Frameworks`, with both SDL license files in `Contents/Resources/licenses`. It removes previous signatures before changing install names/rpaths, replaces SDL references with `@rpath/<name>`, removes old rpaths, and adds only `@executable_path/../Frameworks`. sdl2-compat finds `libSDL3.dylib` beside itself through its existing loader-path search. The bundler refuses non-system/non-bundle dependencies or a Mach-O minimum other than 13.0. It signs actual nested dylibs first, then the app, all with identity `-`; hardened runtime/library validation is not enabled. `codesign --verify --deep --strict` is required before replacement.

Bundle inventory contains our executable, the two SDL dylibs and three relative symlinks, `Info.plist`, the generated `Native.icns`, SDL licenses and the signature resource seal. The icon is created by `tools/cod2x/app_icon.swift`; no Activision artwork is used. No Python resources, game IWDs, extracted shaders, shader manifest, original Mac executable, references, demos, screenshots or CD key are included. Build archives/caches, generated data and `dist/` are ignored and uncommitted.

## First run

Before engine initialization, Cocoa establishes the app home and aligns the engine's `HOME` with Foundation's home. This makes `CFFIXED_USER_HOME` isolate both app data and `.cod2/preferences` in tests.

1. Copy old `CoD2x Native/main`, `shaders`, and `data-path.txt` into the new home if their destinations do not exist. A migration marker prevents later overwriting of new configs. The old directory is retained; every reused shader cache is verified.
2. Find game data using the remembered path, an optional explicit developer plist override, `~/Games/CoD2`, Steam's default library and paths parsed from `libraryfolders.vdf`, or `/Applications/Call of Duty 2*`. Standard app `Contents/Resources`/`Contents` locations are considered. Otherwise show a directory picker, including app packages. Require readable `main/iw_00.iwd` through `iw_15.iwd`. Invalid selections show an `NSAlert` explaining that licensed data is needed, with a button linking to `https://github.com/STiXzoOR/cod2-silicon#game-data`. No release plist contains `CoD2GameDirectory` by default; `--game-dir` is an explicit developer-only choice.
3. Reuse a valid stored CD key or show an `NSSecureTextField` prompt. Normalize spaces/hyphens and check the same CRC-16 used by `CL_CDKeyValidate`. Blank/invalid saved keys require entry again. Write `.cod2/preferences` through an exclusive mode-600 temporary file and rename it, preserving other entries. The engine's native preferences rewrite also sets mode 600, so later in-game changes retain privacy. No supplied key is logged.
4. Verify all 834 shader/constant payloads against the SHA-256 manifest. If missing or damaged, extract natively from the player's symbolized i386 Mac binary. Search the game folder, Steam libraries, `/Applications` and the conventional `~/Games/CoD2-mac-bin` folder, or let the player choose a Mac game folder. No private reference-project path is used at runtime. If no original shader cache can be produced, offer approximate rendering with an explicit lighting/sky warning; log the same outcome. The developer Python extractor stays in the repository but is not bundled or executed by the app.

Native extraction bounds-checks Mach-O commands, sections and symbol/string tables, locates `_D3DXCompileShader`, validates map entries, builds serialized constant tables, stages output, verifies it and replaces the cache under an exclusive setup lock. There are no embedded original shader payloads.

Test-only environment overrides: `COD2_SETUP_NONINTERACTIVE=1`, `COD2_SETUP_GAME_DIR`, `COD2_SETUP_MAC_BINARY`, and `COD2_SETUP_CD_KEY`. Always pair them with `CFFIXED_USER_HOME=<scratch home>`; use a fake key. They are absent from release launch defaults. Existing `COD2_MAC_SHADER_CACHE` overrides still require manifest verification.

## Verification evidence

Uncommitted evidence is under `output/ws21/`. Commands below are prefixed by `taskpolicy -b nice -n 19` when they compile/build.

```sh
scripts/package-release.sh --stabs-binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos --target cod2_macos --parallel 4
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos-codx --target cod2_macos --parallel 4
python3 tests/fixes13/run.py --build build-macos-codx
sh tests/lp64/renderer/run.sh
python3 tests/lp64/game/run.py
python3 tests/lp64/script/run.py
python3 tests/lp64/script/vm_semantics.py
sh tests/cod2x/run.sh
sh tests/cod2x/run_native.sh
python3 tests/perf/run.py
python3 tests/online/run.py build-macos/compile_commands.json
tools/abi/check.sh build-macos/compile_commands.json output/ws21/abi
```

Both client builds and all listed regression suites pass. `fixes13` needs the CoD2x compile database for its timing tests: the default path `build/ws9` is absent in a fresh worktree, and a stock-only database preprocesses away `Cod2x_LimitedFPS`. These were invocation failures, resolved by `--build build-macos-codx`. The first general `cmake --build` also attempted the existing dedicated server and failed on missing renderer/client symbols such as `g_current_bandwidth_setting`, `g_editingField`, `g_gfxV60DllActive`, `s_sundvars` and `vec3_colorintensity`. Dedicated-server linkage was not changed in this packaging workstream; use the client target for the merge gate.

The complete ABI command exits **0**: **620 TUs, 0 errors, 0 mismatches, 0 new**; **218 renderer bindings, 0 table mismatches, 0 named-cast mismatches, 0 unprototyped floating calls**. The final import audit checks **514 import symbols / 1,776 native sites**, with **0 proven extra dereferences and 0 proven missing dereferences**. Evidence: `abi.log`, `abi/functions.json`, `abi/callbacks.json` and `abi/imports.json`.

Native shader comparison:

```sh
python3 tests/packaging/shaders.py \
  "$HOME/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer" \
  "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
python3 tests/packaging/first_run.py
```

Both binaries yield 834 payload files **and `manifest.json` byte-identical** to the Python tool. Tests also pass cache reuse with no binary, corruption detection/repair, malformed manifest rejection and missing-input fallback. Real Cocoa first-run tests pass identity/no baked path/no Python resources, automatic/remembered/custom Steam library data paths, invalid data rejection, private fake key storage, blank-key rejection and migration once. Native URL/event handling, mouse integration, freeze/watchdog and crash-report tests pass. Interactive visual inspection was unavailable: the computer-use API timed out or returned `cgWindowNotFound` for the owned setup fixture. A one-second `sample` showed the main thread waiting in `Cod2xSetupKey -> NSAlert runModal`; no claim of visually inspected picker/key dialogs is made. Owned UI fixtures were stopped/unregistered, with no other processes terminated.

For the final extracted release, `output/ws21/bundle-final-audit.log` records `otool -L`, complete `otool -l`, `vtool -show-build`, `codesign -dvv` and deep strict verification for all three real Mach-O files. Every linked path is `/System/…`, `/usr/lib/…` or `@rpath/…`; each rpath is `@executable_path/../Frameworks`. All three signatures are `adhoc`, with no TeamIdentifier and no hardened runtime. Symlink targets stay in the bundle. The signature remains valid after extraction and runtime setup.

The zip was copied to `output/ws21/final-download`, given a browser-style Safari quarantine xattr, and extracted using `ditto -x -k`. Quarantine propagated into the app (`final-quarantine-before.log`). `spctl --assess -vv` returned **3**, with `CoD2 Silicon.app: rejected`, expected for this unnotarized ad-hoc app. `xattr -dr com.apple.quarantine` removed it (`final-quarantine-after.log`). This does not turn the signature into a Developer ID/notarized signature or make `spctl` accept it.

The **final extracted zip** passes the empty-home smoke with exit 0 (`final-zip-smoke/results.json`): menu reached, `devmap mp_toujane`, menu/map JPEGs, and scripted quit. `DYLD_PRINT_LIBRARIES=1` records both `Contents/Frameworks/libSDL2-2.0.0.dylib` and `Contents/Frameworks/libSDL3.0.dylib`, with no Homebrew SDL load. The final source-installed app passes the same smoke (`fresh-source-smoke/results.json`). The final archive SHA-256 is `44d9f42431e088db2762da3c39eaa7ec0abbde279ed4f8523666b08d0716137f`.

The committed smoke driver starts only its own app executable under `timeout -k 10 90`, supplies licensed paths and a valid fake key through test overrides, uses an empty `CFFIXED_USER_HOME`, reaches the menu, issues `devmap mp_toujane`, saves two `screenshotJPEG` captures and sends `quit`. An initial driver used an unsupported `echo` command as a readiness marker; the production log explicitly reported `Unknown command "echo"`. The corrected driver waits for real menu initialization. The validated captures show the menu and Toujane deathmatch briefing/world. It is a menu/map/quit test, not another combat or performance benchmark.

The install path was tested from a fresh local `git clone --no-hardlinks --single-branch --branch port/package`, updated to the final implementation commit, and installed into `output/ws21/fresh-applications`:

```sh
./scripts/install.sh --prefix /absolute/scratch/Applications \
  --stabs-binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
```

The script auto-found the Steam Mac value binary. Installation exits 0 and the installed app passes the same empty-home menu/Toujane/two-JPEG/scripted-quit smoke with bundled SDL2/SDL3. No real Applications folder is touched. The default fresh-clone invocation without the STABS input exits 1 with a specific prerequisite explanation. Missing CMake and missing developer tools were simulated with a stock-only PATH and process-local invalid `DEVELOPER_DIR`; both exit 1 and print the requested instructions. `shellcheck -x scripts/{build-sdl,macos-common,package-release,install}.sh`, `bash -n`, YAML parse/runner checks, `git diff --check`, and `shasum -a 256 -c dist/SHA256SUMS` pass.

Only `MacPreferences.c` touches a legacy-built source. Its new code is behind `COD2_X64 && __APPLE__`; preprocessing it with `COD2_X64` undefined and, separately, `COD2_X64=0` produces byte-identical output to `df08d50`. Top-level `CMakeLists.txt`, `src/headers/*`, existing blob inputs and protocol code are untouched. An actual 32-bit linked binary comparison was unavailable on this native arm64 host.

## Exact user installation instructions

**Prebuilt release (no CMake, Xcode, Python or Homebrew):** download `CoD2-Silicon-0.1.0-macos-arm64.zip` and `SHA256SUMS` from the published GitHub release into the same folder. In that folder:

```sh
shasum -a 256 -c SHA256SUMS
ditto -x -k CoD2-Silicon-0.1.0-macos-arm64.zip .
mkdir -p "$HOME/Applications"
ditto "CoD2 Silicon.app" "$HOME/Applications/CoD2 Silicon.app"
open "$HOME/Applications/CoD2 Silicon.app"
```

This is ad-hoc signed and not notarized, so a quarantined browser download is expected to be blocked. On **macOS 15 and later**, first attempt to open it, then open **System Settings → Privacy & Security**, scroll to the security section, select **Open Anyway**, authenticate if requested, and confirm **Open**. The option is available shortly after the blocked attempt. Use this settings flow; do not rely on the former control-click shortcut. [Apple's instructions](https://support.apple.com/102445) describe the current exception flow.

Alternatively, after checking the downloaded checksum and choosing to trust this unsigned build, remove quarantine only from this app and open it:

```sh
xattr -dr com.apple.quarantine "$HOME/Applications/CoD2 Silicon.app"
open "$HOME/Applications/CoD2 Silicon.app"
```

No global Gatekeeper disabling or `sudo` is needed. The local test exercised quarantine removal plus direct app execution, not Finder/System Settings approval or App Translocation. On first run, choose licensed game data if it was not found, enter your own CD key, and choose a licensed Mac executable folder if original shaders were not found. Windows game data works with approximate shaders if no Mac executable/cache is available.

**From source:** install Apple's Command Line Tools with `xcode-select --install`. Install CMake from [cmake.org](https://cmake.org/download/) and add its `bin` directory to PATH, or optionally use `brew install cmake`. Clone the public repository and run:

```sh
git clone https://github.com/STiXzoOR/cod2-silicon.git
cd cod2-silicon
./scripts/install.sh \
  --mac-binary "/path/to/your/Call of Duty 2 Multiplayer" \
  --stabs-binary "/path/to/your/licensed/cod2mp_mac_1.3_i386"
open "$HOME/Applications/CoD2 Silicon.app"
```

Use `--prefix /path/to/Applications` to install elsewhere. `COD2_VALUES_BINARY` / `COD2_STABS_BINARY` can supply the same inputs. SDL downloads/builds are automatic and verified. Python is used only at build time from the installed developer tools; it is not a runtime prerequisite. Locally built apps normally do not acquire browser quarantine; if one was copied through a quarantining download mechanism, use the same macOS 15+ settings exception or app-specific xattr removal above.

## Contract gaps and merge notes

1. **Public source builds are not input-free.** `./scripts/install.sh` alone cannot build for a stranger without `COD2_STABS_BINARY`/`--stabs-binary`. I tested using the Steam executable for both generator inputs; `generate.py` fails with `sGerman_ISO_VK_Map: scalar recovery requires a unique debug layout`. Fixing or replacing the upstream typed-data input contract requires a separate audited datagen workstream. I did not commit generated binary-derived data, type dumps or a licensed executable as a workaround. WS20 must document this extra requirement or avoid promising an input-free source install until it is resolved.
2. **Hosted release CI has the same prerequisite.** `release.yml` triggers on `v*`, uses GitHub's standard Apple silicon `macos-15` runner, builds with the packaging script and uploads only zip/checksums to a **draft** release. It expects owner-configured `COD2_STABS_URL` and `COD2_VALUES_URL` secrets for private HTTPS downloads of the licensed generation inputs, verifies their known SHA-256, and deletes them in an always-run cleanup step. Inputs are never committed or uploaded as release assets. Without those secrets it fails early with an explanation. The runner selection follows [GitHub's runner documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-where-workflows-run). YAML/shell validation passed locally; no hosted workflow, tag push, secret configuration or release upload was performed. Publication remains the orchestrator's action.
3. Older macOS/physical second-Mac, interactive dialog visuals, Finder approval and App Translocation were not available/verified here. The minimum/paths/signatures and real game runs are proven locally as described; those gaps must not be presented as separate-machine certification.
4. The default all-target native build still fails on the pre-existing dedicated-server link. Both requested client targets and their regression gate pass. No dedicated-server repair or performance tuning was included.

Focused implementation commits: `01d341c` native shaders; `1770d82` first-run paths/key/branding; `ad85ad2` SDL/bundler/scripts/workflow; `1b92965` smoke readiness; `9fb22bc` CRC/release-target safeguards; `e921475` installer OS-version check. Rebuild release artifacts after merging. No binary artifact, archive, shader cache, capture or CD key is committed.

Shared-file risk is limited: no top-level CMake/header edits; native-only CMake changes are self-contained. `macos_display.c` changes only the title and diagnostic prefixes; preserve WS19's pacing edits when resolving those lines. `MacPreferences.c` adds four guarded lines to preserve private key permissions. `cod2x_native_setup.h` is substantially replaced, and new extraction/test files are independent. Do not overwrite or remove the old installed app. All owned test bundles are unregistered after runs. Two failed disposable Cocoa fixtures had already been deleted before deregistration; their exact owned temporary URLs were briefly restored from our app, deregistered, then removed again. The final LaunchServices dump contains none of the fixture, source-install or final-zip URLs checked during cleanup.
