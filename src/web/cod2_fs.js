(function (global) {
  'use strict';

  var IDB_NAME = 'cod2-web-fs-v1';
  var IDB_STORE = 'kv';
  var KEY_DIR_HANDLE = 'install-directory-handle';
  var KEY_CACHE_META = 'opfs-cache-meta';
  var OPFS_ROOT = 'cod2';

  var WANTED_EXT = ['.iwd', '.cfg', '.dat', '.txt', '.menu', '.so', '.dll',
                    '.bik', '.roq', '.str', '.gsc', '.csc', '.arena'];

  var WANTED_DIRS = ['main', 'zone'];

  function openDb() {
    return new Promise(function (resolve, reject) {
      var req = indexedDB.open(IDB_NAME, 1);
      req.onupgradeneeded = function () { req.result.createObjectStore(IDB_STORE); };
      req.onsuccess = function () { resolve(req.result); };
      req.onerror = function () { reject(req.error); };
    });
  }
  function idbGet(key) {
    return openDb().then(function (db) {
      return new Promise(function (resolve, reject) {
        var tx = db.transaction(IDB_STORE, 'readonly');
        var rq = tx.objectStore(IDB_STORE).get(key);
        rq.onsuccess = function () { resolve(rq.result); };
        rq.onerror = function () { reject(rq.error); };
        tx.oncomplete = function () { db.close(); };
      });
    });
  }
  function idbSet(key, value) {
    return openDb().then(function (db) {
      return new Promise(function (resolve, reject) {
        var tx = db.transaction(IDB_STORE, 'readwrite');
        tx.objectStore(IDB_STORE).put(value, key);
        tx.oncomplete = function () { db.close(); resolve(); };
        tx.onerror = function () { db.close(); reject(tx.error); };
      });
    });
  }
  function idbDel(key) {
    return openDb().then(function (db) {
      return new Promise(function (resolve, reject) {
        var tx = db.transaction(IDB_STORE, 'readwrite');
        tx.objectStore(IDB_STORE)['delete'](key);
        tx.oncomplete = function () { db.close(); resolve(); };
        tx.onerror = function () { db.close(); reject(tx.error); };
      });
    });
  }

  function hasFsAccess() { return typeof global.showDirectoryPicker === 'function'; }
  function hasOpfs() {
    return !!(global.navigator && navigator.storage &&
              typeof navigator.storage.getDirectory === 'function');
  }

  function opfsRoot() { return navigator.storage.getDirectory(); }

  async function opfsDir(root, relPath, create) {
    var dir = root;
    var parts = relPath.split('/').filter(Boolean);
    for (var i = 0; i < parts.length; i++) {
      dir = await dir.getDirectoryHandle(parts[i], { create: !!create });
    }
    return dir;
  }
  async function opfsFileHandle(root, relPath, create) {
    var slash = relPath.lastIndexOf('/');
    var dir = slash >= 0 ? await opfsDir(root, relPath.slice(0, slash), create) : root;
    return dir.getFileHandle(relPath.slice(slash + 1), { create: !!create });
  }
  async function opfsRead(relPath) {
    var root = await opfsRoot();
    var base = await opfsDir(root, OPFS_ROOT, false);
    var fh = await opfsFileHandle(base, relPath, false);
    return fh.getFile();
  }
  async function opfsClearCache() {
    var root = await opfsRoot();
    try { await root.removeEntry(OPFS_ROOT, { recursive: true }); } catch (e) {  }
    await idbDel(KEY_CACHE_META);
  }

  function isWantedFile(name) {
    var lower = name.toLowerCase();
    for (var i = 0; i < WANTED_EXT.length; i++) {
      if (lower.endsWith(WANTED_EXT[i])) return true;
    }
    return false;
  }

  async function resolveInstallRoot(handle) {

    for await (var entry of handle.values()) {
      if (entry.kind === 'directory' && entry.name.toLowerCase() === 'main') {
        return handle;
      }
    }

    return null;
  }

  async function scanInstall(pickedHandle) {
    var entries = [];
    var installRoot = await resolveInstallRoot(pickedHandle);
    var pairs;
    if (installRoot) {
      pairs = [{ dir: installRoot, prefix: '' }];
    } else {

      pairs = [{ dir: pickedHandle, prefix: 'main' }];
    }

    async function walk(dirHandle, prefix, depth) {
      for await (var entry of dirHandle.values()) {
        var rel = prefix ? prefix + '/' + entry.name : entry.name;
        if (entry.kind === 'file') {
          if (isWantedFile(entry.name)) {
            var f = await entry.getFile();
            entries.push({ rel: rel, handle: entry, size: f.size, file: f });
          }
        } else if (entry.kind === 'directory') {
          var top = rel.split('/')[0].toLowerCase();

          if (depth === 0 && WANTED_DIRS.indexOf(top) < 0) continue;
          if (depth < 8) await walk(entry, rel, depth + 1);
        }
      }
    }
    for (var i = 0; i < pairs.length; i++) {
      await walk(pairs[i].dir, pairs[i].prefix, 0);
    }
    return entries;
  }

  function looksLikeInstall(entries) {
    for (var i = 0; i < entries.length; i++) {
      var r = entries[i].rel.toLowerCase();
      if (r.indexOf('main/') === 0 && r.endsWith('.iwd')) return true;
    }
    return false;
  }

  async function copyToOpfs(entries, onProgress) {
    var root = await opfsRoot();
    var base = await opfsDir(root, OPFS_ROOT, true);

    var bytesTotal = 0, bytesDone = 0;
    for (var i = 0; i < entries.length; i++) bytesTotal += entries[i].size;

    var meta = { ready: false, files: [] };
    for (var j = 0; j < entries.length; j++) {
      var e = entries[j];
      var skip = false;

      try {
        var existing = await opfsFileHandle(base, e.rel, false);
        var ef = await existing.getFile();
        if (ef.size === e.size) skip = true;
      } catch (err) {  }

      if (onProgress) onProgress({
        phase: skip ? 'skip' : 'copy', fileIndex: j + 1, fileCount: entries.length,
        fileName: e.rel, bytesDone: bytesDone, bytesTotal: bytesTotal
      });

      if (!skip) {
        var fh = await opfsFileHandle(base, e.rel, true);
        var writable = await fh.createWritable();

        await e.file.stream().pipeTo(writable);
      }
      bytesDone += e.size;
      meta.files.push({ path: e.rel, size: e.size });
      if (onProgress) onProgress({
        phase: 'progress', fileIndex: j + 1, fileCount: entries.length,
        fileName: e.rel, bytesDone: bytesDone, bytesTotal: bytesTotal
      });
    }
    meta.ready = true;
    await idbSet(KEY_CACHE_META, meta);
    if (typeof navigator.storage.persist === 'function') {
      try { await navigator.storage.persist(); } catch (e) {  }
    }
    return meta;
  }

  async function queryPerm(handle) {
    if (!handle || typeof handle.queryPermission !== 'function') return 'denied';
    return handle.queryPermission({ mode: 'read' });
  }
  async function requestPerm(handle) {
    if (!handle || typeof handle.requestPermission !== 'function') return 'denied';
    return handle.requestPermission({ mode: 'read' });
  }

  async function enumerateInstall(handle) {
    var entries = await scanInstall(handle);
    if (!looksLikeInstall(entries)) {
      throw new Error('That folder does not look like a CoD2 install ' +
        '(no main/*.iwd found). Pick the folder that contains "main".');
    }

    await idbSet(KEY_DIR_HANDLE, handle);
    var files = entries.map(function (e) {
      return { path: e.rel, file: e.file, size: e.size };
    });
    return { name: handle.name || 'cod2', files: files };
  }

  var OPFS_HOME = 'home';
  async function opfsHomeDir(create) {
    var root = await opfsRoot();
    return opfsDir(root, OPFS_HOME, create !== false);
  }

  var Cod2Fs = {
    OPFS_ROOT: OPFS_ROOT,
    OPFS_HOME: OPFS_HOME,
    hasFsAccess: hasFsAccess,
    hasOpfs: hasOpfs,

    enumerateInstall: enumerateInstall,
    opfsHomeDir: opfsHomeDir,

    async getCacheMeta() {
      var m = await idbGet(KEY_CACHE_META);
      return (m && m.ready) ? m : null;
    },

    getSavedHandle() { return idbGet(KEY_DIR_HANDLE); },

    queryHandlePermission: queryPerm,
    requestHandlePermission: requestPerm,

    async pickDirectory() {
      if (!hasFsAccess()) {
        throw new Error('This browser lacks the File System Access API ' +
          '(Chromium-only). Use Chrome, Edge, or another Chromium browser.');
      }
      return global.showDirectoryPicker({ id: 'cod2', mode: 'read' });
    },

    async ingestHandle(handle, onProgress) {
      if (onProgress) onProgress({ phase: 'scan' });
      var entries = await scanInstall(handle);
      if (!looksLikeInstall(entries)) {
        throw new Error('That folder does not look like a CoD2 install ' +
          '(no main/*.iwd found). Pick the folder that contains "main".');
      }
      await idbSet(KEY_DIR_HANDLE, handle);
      var meta = await copyToOpfs(entries, onProgress);
      if (onProgress) onProgress({ phase: 'done', fileCount: entries.length });
      return meta;
    },

    readCached: opfsRead,

    clearCache: opfsClearCache,
    forgetHandle() { return idbDel(KEY_DIR_HANDLE); }
  };

  global.Cod2Fs = Cod2Fs;
})(typeof window !== 'undefined' ? window : this);
