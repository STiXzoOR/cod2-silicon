# Web Build

This target builds the port with Emscripten, SDL2, WASM, and WebGL2. With the
[Emscripten SDK](https://emscripten.org) active (`emcc` on PATH):

```sh
emcmake cmake -S . -B build-web
cmake --build build-web -j
```

Output is written to `build-web/` (`cod2.html`, `cod2.js`, `cod2.wasm`, and the
asset-loader `cod2_fs.js`). Serve that directory over HTTP and supply your own
data via the in-page folder picker.

The build keeps no extra state; Emscripten's cache lives under the ignored
build directory.

The target requests a WebGL2/ES3 context with:

```text
-sUSE_SDL=2 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1
```

## Asset loading (point the browser at your CoD2 install)

The page never bundles game assets. On first run it shows a landing screen with
**"Select your Call of Duty 2 folder"**. The loader (`src/web/cod2_fs.js`) then:

1. **Picks the folder** with the File System Access API
   (`showDirectoryPicker`, Chromium-only). Pick either the install root (the
   folder that contains `main/`) or `main/` itself. The folder is validated to
   look like a CoD2 install (it must contain `main/*.iwd`).
2. **Caches assets into OPFS** (Origin Private File System) — the iwd archives
   plus loose `*.cfg` / localization / config files the engine reads off disk.
   OPFS is disk-backed and persistent, so the data survives reloads and is never
   uploaded. A per-file freshness check (same size ⇒ skip) avoids re-copying.
   `navigator.storage.persist()` is requested so the cache is not evicted.
   Progress (per file and total MB) is shown.
3. **Remembers the folder**: the `FileSystemDirectoryHandle` is stored in
   IndexedDB. On reload the handle is read back and `queryPermission({mode:'read'})`
   is checked — if `granted`, the game resumes silently; if `prompt`, a
   **"Resume from …"** button calls `requestPermission`. If the disk handle is
   gone or permission is refused, the game boots straight from the OPFS cache
   (**"Play from cached assets"**). **Change folder** re-runs the picker;
   **Clear cache** wipes OPFS + the saved handle.
4. **Mounts the assets into the engine FS** and launches with
   `+set fs_basepath /cod2 +set fs_homepath /cod2home`, so `FS_Startup` finds
   `/cod2/main/*.iwd` and config/savegames persist under `/cod2home`.

Non-Chromium browsers (no File System Access API) fall back to a
`<input webkitdirectory>` picker that streams the selected assets straight into
the engine FS for that session (no folder memory — that needs Chromium).

### Zero-copy direct read (preferred): `make web WORKERFS=1`

This variant reads the iwd archives **directly from the picked disk folder** —
**no copy into OPFS, no full-RAM load**. The folder is the live source; bytes are
sliced on demand as the engine seeks the ZIP central directory and per-file data.

How it works:

1. The shell (`src/web/shell.html`) calls `Cod2Fs.enumerateInstall(handle)`, which
   validates the folder and returns live `File` objects from the picked
   `FileSystemDirectoryHandle` (it also remembers the handle in IndexedDB). No
   bytes are read or copied at this point.
2. It mounts those File objects read-only with **WORKERFS**:
   `FS.mount(WORKERFS, { files: [{name:'main/iw_00.iwd', data:File}, …] }, '/cod2')`.
   WORKERFS serves **synchronous** slice reads via `FileReaderSync`, which only
   exists on a **Worker** thread — so `main()` must run on a worker.
3. `-sPROXY_TO_PTHREAD=1` (+ `-pthread`, SharedArrayBuffer) moves `main()` to a
   worker; SDL2's canvas/GL then uses **OffscreenCanvas**
   (`-sOFFSCREENCANVAS_SUPPORT=1`, `-sOFFSCREENCANVASES_TO_PTHREAD=#canvas` to
   transfer `#canvas` at startup).
4. The game launches with `+set fs_basepath /cod2 +set fs_homepath /cod2home`.
   `/cod2/main/*.iwd` resolves to the WORKERFS-mounted File objects.
5. **OPFS is used ONLY for `/cod2home`** (savegames, config, `players/`): the
   shell mirrors the small OPFS `home/` subtree into the writable FS on boot and
   flushes it back on an interval + at `pagehide`. The read-only assets are never
   duplicated.

On reload the remembered handle is restored from IndexedDB and re-granted
(silent if `granted`, one click if `prompt`), then re-mounted — **no re-pick, no
copy**. There is no "play from cached assets" because nothing is cached.

Because of `-pthread`, the page **must be served cross-origin isolated** with the
COOP/COEP headers shown below. `make web-serve` (Python's `http.server`) does NOT
send them — use a server that does.

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Build:

```sh
source /tmp/emsdk/emsdk_env.sh
EM_CACHE=/tmp/emscripten_cache TMPDIR=/tmp make web WORKERFS=1
```

> **Status / honesty note.** The `WORKERFS=1` build links and produces the
> artifacts with the WORKERFS + `PROXY_TO_PTHREAD` + OffscreenCanvas flags. The
> mount wiring matches the emscripten WORKERFS API for the bundled emsdk.
> **In-browser verification (does SDL2's GL renderer run correctly on the worker
> with OffscreenCanvas, and do the synchronous WORKERFS reads satisfy
> `FS_Startup`'s random-access ZIP seeks) is the user's test — it has not been
> run in a real browser here.** If the SDL+GL+OffscreenCanvas combination misbehaves
> in your browser, the default `make web` (MEMFS) and `make web WASMFS=1` paths
> remain available as fallbacks, but those DO copy assets (into RAM / OPFS
> respectively).

### Filesystem mount: MEMFS (default) vs WASMFS+OPFS vs WORKERFS

There are three ways assets reach the C filesystem. **WORKERFS (above) is the
only true zero-copy path.** The two below copy the assets:

* **Default `make web` — legacy MEMFS.** On boot the OPFS-cached files are
  streamed once into the in-memory FS at `/cod2`. Works on **any** static
  server (no special headers). Trade-off: the assets occupy wasm heap RAM
  (`-sALLOW_MEMORY_GROWTH=1` covers it), so trim to the maps you need.

* **`make web WASMFS=1` — WASMFS + OPFS backend.** The C filesystem is backed by
  WASMFS and the OPFS tree is mounted directly at `/cod2`, so the engine reads
  iwd bytes **on demand** from OPFS — the archives are never copied into RAM.
  This is the preferred architecture for full installs. It adds `-sWASMFS`,
  `-DWEB_WASMFS` (activates `cod2_mount_opfs()` in
  `src/web/cod2_wasmfs_mount.c`),
  and **`-pthread`** (the OPFS backend proxies blocking I/O to a dedicated
  worker). Because of `-pthread`, the page **must be served cross-origin
  isolated** — the server has to send:

  ```text
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
  ```

  Without those headers a `-pthread` build will not start. `make web-serve`
  (Python's `http.server`) does **not** send them, so for the WASMFS variant use
  a server that does (e.g. `emrun --no_browser` or a small static server that
  adds the COOP/COEP headers).

## Manual browser test (requires a real browser — cannot be run here)

1. Build and serve:
   ```sh
   source /tmp/emsdk/emsdk_env.sh
   EM_CACHE=/tmp/emscripten_cache TMPDIR=/tmp make web
   make web-serve        # serves build/web/ on http://localhost:8000
   ```
2. Open `http://localhost:8000/cod2.html` in **Chrome or Edge**.
3. Click **"Select your Call of Duty 2 folder"** and choose your CoD2 install
   (the folder containing `main/`). Approve the read permission.
4. Watch the copy progress (per-file + total MB). When it finishes the canvas
   appears and the game starts with `fs_basepath=/cod2`.
5. **Reload the page.** It should resume **without re-picking** — either silently
   (permission still granted) or via the **"Resume from …"** button. Disconnect
   the drive / move the folder and reload: it should offer **"Play from cached
   assets"** and boot from OPFS.
6. Use **Clear cache** to reset, and **Change folder** to point at a different
   install.
7. (Optional) Verify the OPFS cache exists: DevTools → Application → Storage →
   *Origin Private File System* should show `cod2/main/*.iwd`.
8. For the WASMFS on-demand variant: `make web WASMFS=1`, serve with COOP/COEP
   headers, and confirm it boots without copying the iwds into RAM.

### Manual browser test — WORKERFS zero-copy (`make web WORKERFS=1`)

This is the no-copy path. The page must be **cross-origin isolated** (COOP/COEP).

1. Build:
   ```sh
   source /tmp/emsdk/emsdk_env.sh
   EM_CACHE=/tmp/emscripten_cache TMPDIR=/tmp make web WORKERFS=1
   ```
2. Serve `build/web/` over HTTP **with COOP/COEP headers** (plain `http.server`
   will not work — pthreads need cross-origin isolation). For example a tiny
   Python server that adds:
   ```text
   Cross-Origin-Opener-Policy: same-origin
   Cross-Origin-Embedder-Policy: require-corp
   ```
3. Open `http://localhost:PORT/cod2.html` in **Chrome or Edge**. Confirm the
   console shows the runtime initializing on a worker (PROXY_TO_PTHREAD).
4. Click **"Select your Call of Duty 2 folder"** and pick
   your Call of Duty 2 install directory (the folder containing `main/`). Grant
   read permission.
   It should say "Reading your install…" then "Mounting N files (read-only, no
   copy)…" and start — **with NO copy progress bar**, because nothing is copied.
5. Verify zero-copy: DevTools → Application → Storage → *Origin Private File
   System* should contain **only `home/`** (config/saves) and **NO `cod2/`**
   asset cache. The wasm heap should not balloon by the size of the iwds.
6. **Reload the page.** It resumes from the remembered handle — silently if
   permission is still `granted`, or via **"Resume from …"** (one click) if
   `prompt`. **No re-pick, no copy.**
7. **Change folder** re-runs the picker and re-mounts. **Clear cache** forgets
   the handle (and clears the OPFS `home/` profile).

## Preloading (alternative)

To package assets into Emscripten's virtual filesystem at build time instead,
pass paths through `WEB_PRELOAD`:

```sh
make web WEB_PRELOAD="main"
```

Preloading the full `main` directory is large; for iteration, prefer a trimmed
asset directory.
