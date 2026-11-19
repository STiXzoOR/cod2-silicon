# Building for Windows

MinGW builds the same engine as the macOS and Linux targets (`cmake/windows-native.cmake`),
for x86_64 or i686. Pick x86_64 unless you have a reason not to: it runs the same pointer-width
paths as those targets, and its replay matches macOS bit for bit.

| Output | Option | What it is |
| --- | --- | --- |
| `cod2_win32_ded.exe` | default | Dedicated server |
| `cod2_win32.exe` | `-DCOD2_WIN32_CLIENT=ON` | SDL2/OpenGL client |
| `cod2_win32.exe` | `-DCOD2_WIN32_CLIENT=ON -DCOD2_WIN32_D3D9=ON` | Client that renders through the system `d3d9.dll` |

## Cross-compiling with Docker (tested)

These commands were run as written, on macOS arm64 with Docker emulating `linux/amd64`. Run them
from the repository root. The builds go in a Docker volume, not the repository.

```sh
docker build --platform linux/amd64 -t cod2-build-win tools/windows
docker volume create cod2-win-build

# Dedicated server
docker run --rm --platform linux/amd64 -v "$PWD":/src:ro -v cod2-win-build:/w cod2-build-win bash -c '
  cmake -S /src -B /w/ded64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mingw64.cmake &&
  cmake --build /w/ded64 --parallel 8'

# Client. Configure downloads SDL2 2.30.9 from libsdl-org and the curl 8.14.1 source from curl.se,
# each checked against a pinned SHA-256, and builds libcurl.
docker run --rm --platform linux/amd64 -v "$PWD":/src:ro -v cod2-win-build:/w cod2-build-win bash -c '
  cmake -S /src -B /w/cli64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mingw64.cmake -DCOD2_WIN32_CLIENT=ON &&
  cmake --build /w/cli64 --parallel 8'
```

For 32-bit, use `cmake/toolchain-mingw32.cmake`. To copy a result out:

```sh
docker run --rm --platform linux/amd64 -v cod2-win-build:/w -v "$PWD/out-win":/out cod2-build-win \
  cp /w/cli64/cod2_win32.exe /out/
```

The executables link SDL2, zlib and the C++ runtime statically, so they need only system DLLs.
Put one in the game folder, next to `main/`, and start it there. Game data is never part of this
repository.

## Running under Wine (tested)

Wine is in the same image. Game data goes in read-only. The client needs a display and software
OpenGL, which these commands do not install; see the note below.

```sh
docker run --rm --platform linux/amd64 -v "$PWD":/src:ro -v cod2-win-build:/w \
  -v "$HOME/Games/CoD2":/data:ro cod2-build-win bash -c '
  export WINEPREFIX=/w/wine64 WINEARCH=win64 WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
  wineboot -i
  mkdir -p /w/run/home/main
  cd /w/run && wine /w/ded64/cod2_win32_ded.exe +set fs_basepath /data +set fs_homepath /w/run/home \
    +set dedicated 2 +set sv_punkbuster 0 +set net_ip 127.0.0.1 +set net_port 28960 +set g_gametype dm +map mp_toujane'
```

For a 32-bit build, use a separate prefix with `WINEARCH=win32`.

The replay in `tools/parity/record.py` runs a Windows build through `tools/windows/wine-binary.sh`:

```sh
docker run --rm --platform linux/amd64 -v "$PWD":/src:ro -v cod2-win-build:/w \
  -v "$HOME/Games/CoD2":/data:ro -v /path/to/recording:/evidence:ro cod2-build-win bash -c '
  export WINEPREFIX=/w/wine64 WINEARCH=win64 COD2_EXE=/w/ded64/cod2_win32_ded.exe
  python3 /src/tools/parity/record.py --binary /src/tools/windows/wine-binary.sh --data /data \
    --output /w/run/replay --port 29673 --dedicated --players --clients 2 --frames 2000 \
    --timeout 550 --replay /evidence/usercmds.txt'
```

Compare its `players.txt` with another build's line by line after normalising line endings. A byte
comparison will report a difference, because Windows writes CRLF line endings.

To see the client without a monitor, also install `xvfb libgl1-mesa-dri libgl1-mesa-dri:i386
libgl1:i386 libglx-mesa0:i386` and the i386 X11 libraries in the container, start `Xvfb`, and set
`DISPLAY`, `LIBGL_ALWAYS_SOFTWARE=1` and `GALLIUM_DRIVER`. Under Rosetta, use `softpipe` on x86_64,
because `llvmpipe` draws a green sky there with the same engine state. Use `llvmpipe` on i686, because
`softpipe` got stuck in one draw call while loading a map and stayed there for the full 8-minute run.

On an arm64 Mac, `rosetta error: invalid gdt selector index 5` from an x86_64 build means the game
faulted, or was killed by `timeout`, and Rosetta hid the crash report. Reproduce on i686, whose crash
handler works there.

## Direct3D 9 (optional)

`-DCOD2_WIN32_D3D9=ON` makes the client create a real Direct3D 9 device on its SDL window, as
Windows 1.3 does, with no OpenGL context. It compiles the game's own HLSL from `main/` at material
load with `D3DCompile` from `d3dcompiler_47.dll` (then `_46`, `_43`), which Windows 10 and 11 ship.
The option is off by default, and the OpenGL client is unchanged with it off.

```sh
docker run --rm --platform linux/amd64 -v "$PWD":/src:ro -v cod2-win-build:/w cod2-build-win bash -c '
  cmake -S /src -B /w/d3d64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mingw64.cmake \
    -DCOD2_WIN32_CLIENT=ON -DCOD2_WIN32_D3D9=ON &&
  cmake --build /w/d3d64 --parallel 8'
```

What was run, x86_64 only:

- **Windows 11 in a virtual machine**, with Microsoft's `d3d9.dll` and compiler and software
  rendering: the menu, a map, `vid_restart`, a mode change and exclusive fullscreen. Every shader
  the menu and `mp_toujane` need compiled. No machine with a graphics card has run it.
- **Wine with DXVK:** four maps in the default and Direct3D 7 paths. Wine's own HLSL compiler
  rejects a few of the game's shaders (which ones depends on the Wine version), so those runs used
  bytecode compiled from the same HLSL with vkd3d-shader. Expect missing materials under Wine
  without that.

The i686 build of this option has not been run.

## Building on Windows (not tested)

Nobody has run these steps. The project has only been built by cross-compiling and run under Wine.

**MSYS2.** In the UCRT64 shell (MINGW32 for a 32-bit build, with the matching package prefix):

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-zlib mingw-w64-ucrt-x86_64-python
cmake -S . -B out -G Ninja
cmake --build out
cmake -S . -B out-client -G Ninja -DCOD2_WIN32_CLIENT=ON
cmake --build out-client
```

The toolchain files are for cross-compiling, so leave them out here. The client's configure step
needs `sh`, `curl`, `sha256sum` and `tar`, which an MSYS2 shell has.

**MSVC.** The `msvc-*` presets in `CMakePresets.json` are separate and unfinished. They do not build
this engine path: `cmake/win32-msvc.cmake` compiles the older plain path, and its x64 preset is
compile-only.

## What is missing

- **Downloading from a server** is in, but only half tested. The client links a static libcurl
  that configure builds from pinned, checksummed source (`tools/windows/fetch-curl.sh`), with
  Windows' own TLS. Under Wine the library fetched a file over HTTP and HTTPS and reported a
  missing one, using the calls the game makes. A download through the game itself needs a
  Windows install with a CD key to join a server, and was tested on macOS only.
- **The original launcher behaviour:** the safe-mode prompt after a crash, the second-copy check,
  the auto-update check and IPX.
- **The CD key** comes from the registry as on Windows 1.3 (the 32-bit view and the
  VirtualStore), then from the preferences file. **Preferences** use a file under `%HOME%`.
- **Voice chat.** The client is built without it (`src/win32/win_voice_none.c`): the DirectSound
  layer of Windows 1.3 is not in this source tree yet. A checkout that has one lists its files in
  `cmake/windows-voice.cmake` as `COD2_WIN_VOICE_C`, and the build uses them.

## Sound

Windows uses the same mixer as macOS, `src/platform/macos_audio.c`, with SDL playing the output and
reading WAV, and `third_party/minimp3` reading MP3. The original loads `mss32.dll`, which is
closed-source and 32-bit only.

To check sound with no audio hardware, send it to a file with `SDL_AUDIODRIVER=disk` and
`SDL_DISKAUDIOFILE=sound.raw`. The file is 44.1 kHz stereo 32-bit float.

## The older plain path

`-DCOD2_WIN32_LEGACY=ON` builds the non-`COD2_X64` branches with the 32-bit data blobs. It links and
boots, but compiles stale logic: collision, menus, HUD and more differ from Windows 1.3.
