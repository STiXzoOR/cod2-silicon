# opencod2

> A reconstruction of the **Call of Duty 2** engine

> [!WARNING]
> **Work in progress — this does not fully work yet.** It is an early, incomplete
> reconstruction: it builds and boots, but expect crashes, missing functionality,
> and broken features. It is shared for the curious and for collaboration, not as
> a finished or playable port. No stability, no support, and the code may change
> shape at any time.

> [!IMPORTANT]
> **No game content is included — bring your own data.** This repository is
> *source code only*: no executables, archives, maps, models, textures, sounds,
> or scripts. To run anything you must supply data files from a copy of the game
> that **you legally own**.

## Status

The reconstruction is incomplete. It may crash, omit subsystems, or only
partially implement behavior that exists in the original game.

## Security

Older Call of Duty titles and game engines from this era have a history of
security-sensitive bugs, especially around networking, file parsing,
content-loading paths, and memory safety. This project should not currently be
treated as a hardened or production-safe engine.

One long-term goal of the reconstruction is to make those risks easier to audit
and fix: preserve compatibility where practical, but replace unsafe behavior and
close vulnerabilities as they are found. Until then, run it only with data you
trust and avoid exposing test servers to untrusted networks.

## Building

All targets are driven by **CMake (≥ 3.16)**, each an out-of-source build into
its own directory.

### Linux (primary)

The engine is a 32-bit x86 binary; you need a multilib toolchain and 32-bit libs
(Debian/Ubuntu names shown):

```sh
sudo apt install build-essential gcc-multilib g++-multilib cmake \
     libsdl2-dev:i386 libgl1-mesa-dev:i386 libx11-dev:i386 \
     libcurl4-openssl-dev:i386 zlib1g-dev:i386 libstdc++6:i386
```

```sh
cmake -S . -B build-native
cmake --build build-native -j
# -> build-native/cod2_linux    (client)
# -> build-native/cod2_lnxded   (dedicated server)
```

### Windows (MinGW cross-compile)

```sh
cmake -S . -B build-win32 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake
cmake --build build-win32 -j
# -> build-win32/cod2_win32_ded.exe   (dedicated server)
```

Add `-DCOD2_WIN32_CLIENT=ON` for the SDL2/GL client (supply SDL2 dev libs under
`src/win32/sdl2/lib/`; the dedicated server needs none).

#### Swap-in renderer DLL (optional, experimental)

The renderer can be built as a separate swap-in DLL — it exports `GetRefAPI`
and talks to the engine only through the `ri`/`re` tables. With the client
configured, run the `gfxdll` target:

```sh
cmake --build build-win32 --target gfxdll
# -> build-win32/gfx_d3d_mp_x86_s.dll   (renderer DLL)
# -> build-win32/cod2_win32_gfxdll.exe  (engine that loads it at runtime)
```

The renderer↔engine bridge is pre-generated and committed under `build/gfxdll/`;
the build is pure compile+link. Experimental — not exhaustively tested.

## Running

This reconstructs the engine, not the content. Point it at data from a copy of
the game you legally own:

```sh
./build-native/cod2_linux +set fs_basepath /path/to/your/game
```

Without legally-obtained data the build runs but has nothing to load.

## Notice

This is an independent, source-level reconstruction of the Call of Duty
2 engine. It is not affiliated with, authorized by, sponsored by, or endorsed by
Activision Publishing, Inc., Infinity Ward, or any of their affiliates.

"Call of Duty" and "Call of Duty 2" are trademarks of Activision Publishing,
Inc. They are used in this repository only for identification and
interoperability, to describe what the code reconstructs. No claim is made to
those marks.

This project does not provide or help obtain copyrighted game data. To use it
with real game content, you must supply data files from a copy of Call of Duty 2
that you legally own.

The reconstructed engine source is a derivative work created for the purposes of
preservation, interoperability, research, and education. It is provided as-is,
without warranty of any kind, express or implied. The original port, build
system, and platform glue are separable original work.

If you are a rights holder and believe something here should not be distributed,
please open an issue or contact the maintainer and it will be addressed
promptly.
