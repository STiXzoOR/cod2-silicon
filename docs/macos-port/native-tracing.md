# Native diagnostic tracing

Native release runs do not print reconstruction trace tags or create the
`x64_*_trace.txt` / `cod2_heapdiag.txt` diagnostic files by default.

Set `COD2_NATIVE_TRACE=1` in the process environment before launch to enable
the retained connection, screenshot, UI sweep, combat and renderer trace
output. An unset variable, an empty value or `0` disables tracing. The gate
caches its value once at engine startup; restart the process when changing it.

Trace files use the process working directory. Run diagnostic sessions from
`~/Library/Application Support/CoD2-native-ws42/` and use that directory for
`fs_homepath` as well. The menu-sweep driver sets the opt-in variable itself
and writes its output to its requested evidence directory.

`COD2_NATIVE_TRACE` also enables the `ui_sweep` console command. It replaces
the separate `COD2_UI_SWEEP`, `COD2_MAC_COMBAT_TRACE` and `COD2_HEAPDIAG`
switches. Heap tracing additionally requires the Windows CRT debug build.

The original reconstruction's `COD2_DEBUG_ENV` and `COD2_DEBUG_ONLY` probes
remain compiled out of native release builds. They include flags that alter
renderer behavior and VM corruption probes, so enabling native tracing does
not enable those probes. The legacy build retains its original diagnostics.

Existing measurement tools with explicit output paths (`COD2_FRAME_CSV`,
`COD2_SERVER_TICK_CSV`, `SYSDIFF_STATEHASH` / `SYSDIFF_DUMP`, and the renderer's
draw/shader inspection tools) retain their own opt-ins and stay off by default.

Check the source policy with:

```sh
python3 tools/check_trace_guards.py
python3 -m unittest discover -s tools/tests -p test_trace_guards.py -v
```

The source check follows native release preprocessor branches, ignores
comments, and requires a local opt-in guard around tagged output and named
trace-file opens. Its synthetic tests check unguarded tags/files, unrelated
environment variables, sibling and `else` branches, inverted/OR conditions,
early returns, and preservation of legacy-only probes. It is a lexical guard
check; normal compilation and runtime QA remain part of the full merge gate.
