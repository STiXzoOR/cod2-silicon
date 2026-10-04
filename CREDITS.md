# Credits

CoD2 Silicon is maintained by Neoptolemos Kyriakou and contributors. It rests
on years of reconstruction, compatibility and tooling work by the projects
below. A reference credit does not imply endorsement or inclusion of its code.
[NOTICE.md](NOTICE.md) explains the license boundaries.

Entries were checked against local Git remotes, license files, the port
reports and public repository metadata on 2026-10-04. “No license declared”
means no project-wide grant was found; it is not permission to copy code.
Older research notes sometimes reported no license where the current clone
or metadata declares one. This inventory uses the checked source and notes
mixed provenance where relevant. The detailed evidence is in
[WS20](docs/macos-port/reports/WS20-publish.md).

## Engine and behavior references

| Project / tool | Author or organisation | License | How it was used |
| --- | --- | --- | --- |
| [opencod2](https://github.com/opencod2/opencod2) | opencod2 authors, including riicchhaarrd (Richard) | No license declared | Base code at `410342a`; reconstructed multiplayer engine, Mac renderer and original build paths. |
| [CoD2x](https://github.com/callofduty2x/CoD2x) | callofduty2x and contributors | No license declared | Behavior/protocol reference only, read rather than copied: 1.4.6.8 identity, competitive policy, animation, demos, URLs and Windows baseline. |
| [cod2engine](https://github.com/nawaftahir/cod2engine) | nawaftahir (Nawaf) | No license declared; AGPL-derived lineage | Behavior reference only, never copied: server/game/script control flow and network tables, settled against Mac disassembly. |
| [CoD2rev_Server (upstream)](https://github.com/voron00/CoD2rev_Server) | voron00 and contributors | AGPL-3.0 | Behavior reference only, never copied: LP64 approaches, VM values, bytecode offsets and field tables. |
| [CoD2rev_Server (CoD2x fork)](https://github.com/callofduty2x/CoD2rev_Server) | callofduty2x, voron00 and contributors | AGPL-3.0 | Behavior/protocol reference only, never copied: CoD2x server and earlier x64 changes. |
| [KisakCOD](https://github.com/SwagSoftware/KisakCOD) | SwagSoftware and contributors | GPL-3.0 | Behavior reference only, never copied: CoD4 renderer and script VM intent, verified against CoD2 Mac 1.3. |
| [xtnded/cod2](https://github.com/xtnded/cod2) | CoDExtended / xtnded; riicchhaarrd | GPL-2.0 | Type/lineage reference only, never copied: earlier Mac STABS reconstruction. |
| [cod2-mp-macho](https://github.com/yctn/cod2-mp-macho) | yctn and contributors | No license declared | Reconstruction lineage and format/type provenance research only; no binary or source imported. |
| [cod2_dasm_out](https://github.com/yctn/cod2_dasm_out) | yctn | No license declared | Assembly/reconstruction lineage research only; no dumps imported. |
| [Original Call of Duty 2 / Mac port](https://store.steampowered.com/app/2630/Call_of_Duty_2/) | Infinity Ward / Activision; Mac port by Aspyr / i5works | Proprietary; no redistribution grant | External behavior, ABI, STABS and shader reference. Player-owned Steam data used for runtime verification. No binaries, assets or extracted shaders distributed. |

The local `macbin/cod2mp_mac_1.3_i386` reference is the i386 slice of CoD2x's
`bin/mac/Call of Duty 2 Multiplayer 1.3` (2006 Mac 1.3, full STABS), not the
2013 Steam executable. The latter supplies scalar/shader reference data and
has only partial STABS. opencod2's original blob addresses trace to Mac 1.0
with later 1.3 changes; the research corrected the initial plan's 1.3-only
provenance description. Neither original executable is part of this repo.

## Format, protocol and ecosystem research

| Project / tool | Author or organisation | License | How it was used |
| --- | --- | --- | --- |
| [cod2map](https://github.com/7894752/cod2map) | 7894752 and upstream contributors | No license declared; research notes flag mixed GPL-2-style provenance | Format reference only: BSP/material sizes and limits; no source copied. |
| [cod2map arm64 fork](https://github.com/riicchhaarrd/cod2map) | riicchhaarrd and upstream contributors | No license declared; research notes flag mixed GPL-2-style provenance | External tool/format reference: portable map compiler built outside the repo for research. |
| [CoD2Unity](https://github.com/Hommsin/CoD2Unity) | Hommsin / CptAsgard and contributors | CC0-1.0 | Format reference only: BSP lump order and vertex/triangle layout; no wavelet decoder. |
| [cod2unity fork](https://github.com/BUZDOLAPCI/cod2unity) | BUZDOLAPCI and upstream contributors | No license declared | Format survey only: derivative BSP/IWI parser, not incorporated. |
| [PyD3DBSP](https://github.com/mauserzjeh/PyD3DBSP) | Soma Rádóczi (mauserzjeh) | MIT | Format reference only: BSP, IWI, materials and model layouts. |
| [cod-asset-importer](https://github.com/mauserzjeh/cod-asset-importer) | Soma Rádóczi (mauserzjeh) and contributors | GPL-3.0 | Format survey/reference only, never copied: broad model/material/IWI coverage; external oracle proposed, not claimed run. |
| [iwi2dds (Go)](https://github.com/mauserzjeh/iwi2dds) | Soma Rádóczi (mauserzjeh) | MIT | Format reference only: bitmap and DXT conversion; wavelet comparison still needs the original engine. |
| [d3dbsp](https://github.com/riicchhaarrd/d3dbsp) | riicchhaarrd | GPL-3.0 | Format survey only: render-lump parser, no code copied. |
| [material_util](https://github.com/riicchhaarrd/material_util) | riicchhaarrd | GPL-3.0 | Format survey only: material state-map bit groups, no code copied. |
| [iwi2dds (Python)](https://github.com/riicchhaarrd/iwi2dds) | riicchhaarrd | No license declared | Format survey only; names wavelet formats but does not decode them. |
| [demoparser](https://github.com/KILLTUBE/demoparser) | KILLTUBE and contributors; credits voron00 | No license declared | Demo/protocol reference only; no parser source or sample game demos committed. |
| [cod2_master_serv](https://github.com/voron00/cod2_master_serv) | voron00 and contributors | No license declared | Protocol/ecosystem survey only: Perl master server, not used as a live acceptance server. |
| [zk_libcod](https://github.com/ibuddieat/zk_libcod) | ibuddieat and libcod contributors | MIT declared; unlicensed upstream lineage | Protocol/security reference: version quirks, original server hash and optional diagnostics; no hook source copied. |
| [libcod (M-itch)](https://github.com/M-itch/libcod) | M-itch and contributors | No license declared | Hooking/protocol/security ecosystem survey only. |
| [libcod (voron00)](https://github.com/voron00/libcod) | voron00 and libcod contributors | No license declared | Hooking/protocol ecosystem survey only. |
| [CoD2 Docker server](https://github.com/bgauduch/call-of-duty-2-docker-server) | bgauduch and contributors | MIT | Deployment research and stock server hash cross-check; no Docker baseline was run on the development Mac. |
| [cod2-docker](https://github.com/rutkowski-tomasz/cod2-docker) | Tomasz Rutkowski and contributors | Apache-2.0 (current metadata) | Deployment ecosystem survey only. |
| [Greyhound](https://github.com/Scobalula/Greyhound) | Scobalula and contributors | GPL-3.0 | Format/extractor survey only; later-game focus, not incorporated. |
| [cod_masterserver](https://github.com/yorick1989/cod_masterserver) | yorick1989 and contributors | GPL-3.0 | Protocol/master/auth service survey only; local oracle proposed, not claimed run. |
| [codremaster](https://github.com/schwarz/codremaster) | schwarz and contributors | MIT (current metadata) | Master/auth replacement survey only. |
| [CoD4-DM1](https://github.com/Iswenzz/CoD4-DM1) | Iswenzz and contributors | GPL-3.0 (current metadata) | Demo-format structural survey only; different game. |
| [zPAM](https://github.com/eyza-cod2/zpam3) | eyza-cod2 and contributors | No license declared | Mod/behavior reference and external live compatibility test content on CoD2x servers; no mod assets distributed. |
| [iw2clientdll](https://github.com/xtnded/iw2clientdll) | CoDExtended / xtnded | GPL-2.0 (current metadata) | Historical client-hook survey only; not the modern CoD2x protocol. |
| [codscriptdoc](https://github.com/M-itch/codscriptdoc) | M-itch and contributors | No license declared | GSC documentation ecosystem survey. |
| [Killtube](https://killtube.org/) | Killtube community and individual authors | No site-wide license declared for uploads | Historical format/server provenance research. Community downloads are not a redistribution grant; no binaries imported. |

## Libraries and native frameworks

| Project / tool | Author or organisation | License | How it was used |
| --- | --- | --- | --- |
| [SDL3](https://github.com/libsdl-org/SDL) | Sam Lantinga / SDL contributors | Zlib | Library: window, GL context, event handling; tested version 3.4.16 via Homebrew. |
| [sdl2-compat](https://github.com/libsdl-org/sdl2-compat) | Sam Lantinga / SDL contributors | Zlib | Library: SDL2 API over SDL3, tested version 2.32.72. Its Cocoa implementation also informed input/fullscreen integration. |
| [libcurl / curl](https://curl.se/) | Daniel Stenberg and curl contributors | curl license | Library: native HTTP downloads and HTTPS demo uploads; tool: fetching public documentation. Uses the SDK library. |
| [zlib](https://zlib.net/) | Jean-loup Gailly and Mark Adler | Zlib | Library: archive decompression; macOS links SDK zlib, with inherited bundled sources on legacy targets. |
| [Speex](https://www.speex.org/) | Jean-Marc Valin / Xiph.Org and credited contributors | BSD-3-Clause; per-file notices retained | Library: inherited voice codec sources; native microphone capture is not implemented. |
| [libjpeg-turbo / Independent JPEG Group](https://github.com/libjpeg-turbo/libjpeg-turbo) | libjpeg-turbo contributors; Thomas G. Lane and Guido Vollbeding / IJG | IJG, BSD-3-Clause and Zlib as described in its notices | Library: vendored third_party headers/support for legacy JPEG integration; native JPEG uses ImageIO. This software is based in part on the work of the Independent JPEG Group. |
| [macOS SDK frameworks](https://developer.apple.com/documentation/) | Apple | Apple SDK terms / proprietary system frameworks | Libraries: AppKit, Foundation, CoreFoundation, IOKit, OpenGL/CGL, AudioToolbox/CoreAudio, GameController, CoreGraphics and ImageIO for native adapters. |

## Build, analysis and verification tools

| Project / tool | Author or organisation | License | How it was used |
| --- | --- | --- | --- |
| [CMake](https://cmake.org/) | Kitware and contributors | BSD-3-Clause | Tool: configure builds, compile databases, CTest and app targets. |
| [Apple clang / Xcode Command Line Tools](https://developer.apple.com/xcode/) | Apple; LLVM contributors | Apple toolchain terms; upstream LLVM Apache-2.0 WITH LLVM-exception | Tools: C/C++/Objective-C compilation, SDKs, linker, sanitizer fixtures, nm/otool/llvm-objdump ABI and disassembly checks. |
| [LLDB](https://lldb.llvm.org/) | LLVM / Apple | Apache-2.0 WITH LLVM-exception; Apple distribution terms | Tool: native crash and VM shutdown diagnosis. |
| [Instruments / xctrace / Metal performance tools](https://developer.apple.com/instruments/) | Apple | Apple developer-tool terms / proprietary | Tools: Time Profiler and attempted performance captures; reports distinguish failed capture from usable evidence. |
| [Swift](https://www.swift.org/) | Swift project / Apple and contributors | Apache-2.0 with Runtime Library Exception; Apple distribution terms | Tool: SDK-only RGB image checks, parity panels and original geometric app icon; no Pillow/NumPy dependency was used. |
| [Python](https://www.python.org/) | Python Software Foundation and contributors | PSF-2.0 and included historical licenses | Tool: STABS/data generation, ABI audit, test drivers, shader extraction, frame analysis and release documentation. |
| [Git](https://git-scm.com/) | Git contributors | GPL-2.0 | Tool: isolated workstreams, history, comparisons and merge tracking. |
| [GitHub CLI](https://cli.github.com/) | GitHub and contributors | MIT | Tool: public repository/license metadata verification. |
| [GitHub Actions / checkout / upload-artifact](https://github.com/features/actions) | GitHub / actions contributors | Hosted service terms; checkout and upload-artifact MIT | Tools: public build/test CI and diagnostic-log artifacts. |
| [Homebrew](https://brew.sh/) | Homebrew contributors | BSD-2-Clause | Tool: pre-existing CMake and SDL development libraries; no local package installation in WS20. |
| [ShellCheck](https://www.shellcheck.net/) | Vidar Holen (koalaman) and contributors | GPL-3.0 | Tool: shell verification harness lint. |
| [GCC / GNU binutils](https://gcc.gnu.org/) | GNU / Free Software Foundation and contributors | GPL-3.0-or-later tool licenses; runtime exceptions where applicable | Tools: Linux multilib byte-for-byte reference gate; original GNU toolchain matching research, not native Apple compilation. |
| [MinGW-w64](https://www.mingw-w64.org/) | MinGW-w64 contributors | Component-specific: public-domain, ZPL-2.1 and other notices | Tool: inherited Windows cross-build path, preserved rather than exercised on this Mac. |
| [MSVC / Visual Studio](https://visualstudio.microsoft.com/) | Microsoft | Proprietary Microsoft toolchain terms | Tool: inherited native Windows build path, preserved rather than exercised on this Mac. |
| [Emscripten](https://emscripten.org/) | Emscripten contributors | MIT / University of Illinois-NCSA | Tool: inherited WebAssembly target; not claimed rebuilt during this port. |
| [DepotDownloader](https://github.com/SteamRE/DepotDownloader) | SteamRE and contributors | GPL-2.0 | Tool: native arm64 download of the player’s entitled Steam Windows/Mac depots outside git. |
| [Steam / SteamCMD](https://developer.valvesoftware.com/wiki/SteamCMD) | Valve | Proprietary Valve terms | Tools: owned game/depot provenance and Wine Steam-client experiments; SteamCMD documented as an alternative, not claimed run by WS5. |
| [Ruby / Psych](https://github.com/ruby/psych) | Yukihiro Matsumoto and Ruby contributors; Aaron Patterson, SHIBATA Hiroshi and Charles Oliver Nutter / Psych | Ruby OR BSD-2-Clause; Psych MIT | Tool: installed Ruby/Psych parsed CI and issue-template YAML when PyYAML and actionlint were unavailable. |
| [macOS signing and system utilities](https://developer.apple.com/documentation/security) | Apple and component contributors | macOS distribution/component terms | Tools: codesign, plutil, xattr, taskpolicy, shell and checksum utilities for app validation and low-priority development commands. |

## Windows baseline tools (external only)

| Project / tool | Author or organisation | License | How it was used |
| --- | --- | --- | --- |
| [Wine](https://www.winehq.org/) | Wine project contributors | LGPL-2.1-or-later | Tool: execute original Windows CoD2 + CoD2x for the WS8 baseline and WS15 visual reference; not a native runtime dependency. |
| [Highball](https://github.com/gauthierpiarrette/highball) | Gauthier Piarrette and contributors | GPL-3.0 | Tool: external Wine bottles/runtime selection, version 0.10.3. |
| [Sikarugir](https://github.com/Sikarugir-App/Sikarugir) | Sikarugir-App and contributors | Wrapper: no license declared; Wine engine LGPL-2.1-or-later | Tool: external x64-sikarugir10.0_6-r19 engine and D9VK component; GUI wrapper was not installed. |
| [Gcenx macOS Wine builds](https://github.com/Gcenx/macOS_Wine_builds) | Gcenx; Wine contributors | Build repo: no license declared; Wine LGPL-2.1-or-later | Tool: alternative Wine 11.18 backend trials, not successful high-FPS acceptance. |
| [DXVK / D9VK macOS / Kegworks builds](https://github.com/Gcenx/DXVK-macOS) | Philip Rebohle (doitsujin), Joshua Ashton and contributors; Gcenx / Kegworks | Zlib for DXVK | Tools: external modern/legacy D3D9 translation trials; Sikarugir D9VK supplied matched WS15 Windows views. |
| [MoltenVK](https://github.com/KhronosGroup/MoltenVK) | Khronos Group / The Brenwill Workshop and contributors | Apache-2.0 | Library/tool in the external Wine/D9VK Vulkan baseline; failure traces were not labeled valid performance results. |
| [Rosetta 2](https://support.apple.com/en-us/102527) | Apple | Proprietary Apple terms | Tool: external x86 Windows/Wine baseline only; native CoD2 Silicon does not require translation. |
| [DXMT](https://github.com/3Shain/dxmt) | Feifan He / CodeWeavers and contributors | LGPL-2.1-or-later | Downloaded Highball runtime component; its selection routed this D3D9 game through D9VK. No separate DXMT result is claimed. |
| [D3DMetal / Game Porting Toolkit](https://developer.apple.com/games/game-porting-toolkit/) | Apple | Proprietary Apple terms | Bundled in the external runtime but not enabled; its license was not accepted in WS8. No use or benchmark result is claimed. |

## How this was built

Most port code and analysis was written by AI agents: **OpenAI Codex CLI on
`gpt-6.1-sol`** and **Anthropic Claude Code**. Neoptolemos Kyriakou orchestrated
21 workstreams with a merge gate covering both native builds, all test suites,
an ABI check and a live smoke run. The integrated result was verified by live
play on real stock and CoD2x servers, matched rendering captures and a local
combat soak. The reports preserve unverified claims and remaining acceptance
gaps; the public CI subset cannot replace licensed-data and live checks.

| AI tool | Organisation | License / terms | Use |
| --- | --- | --- | --- |
| [Codex CLI](https://github.com/openai/codex) / `gpt-6.1-sol` | OpenAI | CLI Apache-2.0; model service terms | Code, analysis, tests and documentation across workstreams. |
| [Claude Code](https://github.com/anthropics/claude-code) | Anthropic | Proprietary service/tool terms; public repo declares no license | Code and analysis in the orchestrated development workflow. |

The contribution policies also use [Contributor Covenant 2.1](https://www.contributor-covenant.org/version/2/1/code_of_conduct/)
by Coraline Ada Ehmke and contributors (CC-BY-4.0), the
[Developer Certificate of Origin 1.1](https://developercertificate.org/) by
The Linux Foundation and contributors (verbatim redistribution allowed), and
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) by Olivier Lacan and
contributors (MIT), as policy/documentation references.
