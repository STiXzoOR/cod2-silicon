# Renderer shader compiler reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 intentional.** This covers only `D3DXCompileShader` at Mac
0x118aa6. The external shader cache and explicit approximate-rendering fallback
replace the original embedded map. This is not a claim that the approximate
shaders reproduce the original rendering. Existing shader lifetime and constant
table function verdicts are unchanged.

## Original map and live callers

The exact-symbol initializer contains 1,042 unique insertions: 208 `.vsa`, 208
`.vsn`, 209 `.pse`, 208 `.vc` and 209 `.pc` entries. Every insertion's value/key
construction and assignment were checked by script. Initialization sets `sInit`
only after the last insertion. Hand comparison covered initialization, lookup,
output construction, missing entries and the C++ cleanup paths.

At 0x134129 the compiler treats `pSrcData` as a case-sensitive filename, ignoring
the source length, HLSL entry point, profile, defines, include object and flags.
A `.vs` substring takes precedence over `.ps`: vertex keys gain `n` for card
type 2 and `a` otherwise; pixel keys gain `e`. A hit returns `key::program` with
its terminator included in the buffer size. Both optional result objects are
null. A miss at 0x134325 returns success with a null shader and null optional
outputs, allowing the material loader's separate NULL-shader diagnostic.

`Material_LoadPassShader` is the only production caller of the native
`MacD3DXCompileShader` wrapper. It passes the original filename and the profile
derived from the material's shader stage. Mac 0x103c50–0x103cb1 replaces the last
five filename bytes with `.vs` or `.ps` before the compiler lookup. The native
wrapper now performs that transformation, including nonstandard five-byte tails,
and applies the original substring precedence. Normal `.hlsl` lookups retain
their deployed mapping. Short or oversized names receive a bounded native
failure instead of the original string exception or unbounded storage.

## Retained replacement and corrections

The native Apple build excludes the old `Mac/Tools/MacDisplay.c` provider;
`platform/macos_display.c` returns card type zero. Its external cache therefore
retains all 417 selected ARB programs and their 417 constant tables, omitting
the 208 unused NVIDIA programs. All omitted programs differ from their ATI
counterparts; they were checked, not assumed identical. Each retained ARB
payload matches its original insertion byte-for-byte. All 2,605 constant
records match the constructor at 0x13d094 in their used fields, names and
offsets. The compact tables omit the constructor's unused allocation tail
(28 + 292 bytes per constant versus the used records and names). The symbolized
reference, local Steam binary and installed external cache agree on every
retained payload.

The native result uses bare ARB text and a `#COD2CTAB` marker instead of the
original key prefix. The selected device and constant-table providers consume
that representation. Removing unused vertex output declarations remains the
documented Apple GL adaptation. Mutable cache contents retain filename, size,
ARB-header and reflection-offset validation. No payload, disassembly or cache
manifest was committed.

Three live contracts were corrected under `COD2_X64`: filename transformation
and substring precedence (0x103cb1/0x134169), successful null output for absent
entries (0x134325), and final string size after removing unused declarations
(0x13423f). The latter previously copied stale trailing bytes beyond the
serialized string. The existing fixture now supplies real caller-style `.hlsl`
filenames and expects success with a null shader for missing entries.

Without `COD2_MAC_SHADER_CACHE`, the wrapper calls the existing source-based
generic compiler. Packaged CoD2x setup verifies all 834 cache files and, if
unavailable, offers “Use Approximate Shaders” with a lighting/sky warning;
`cod2x_native_setup.h` then clears the environment variable and logs that choice.
This role is also documented in `docs/macos-port/game-data.md`. Stock or direct
launches that bypass setup can reach the same fallback whenever the variable is
absent; the compiler prints a one-time generic-shader notice. That route remains
an intentional approximation with empty reflection, not original-map parity.

## Verification and limits

The actual arm64 Release `D3DXShader.c` object compiled with Apple client flags.
The worker's dedicated Release target built with two low-priority jobs; it
excludes this renderer file, so the separate object supplies renderer compile
coverage. The existing `shader_cache` fixture passed with ASan/UBSan, exercising
cache loading, output-declaration removal, reflection validation and missing
entries. No new cases or assertions were added. It does not cover generic
fallback appearance, all real shader uploads, or assert the final buffer size.

All five `COD2_X64=OFF` preprocessing configurations match the review base,
including actual macro expansions and an additional Apple-SDK-on comparison.
Neither the edited header nor its unchanged including source uses `__LINE__`
or `COD2_ASSERT_FIELD`. No full CI, replay or GL/game run was performed. The
filename edge cases and buffer-size correction are compiled and reviewed, not
individually exercised. No assigned entry remains unchecked.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-shader-compiler.noindex/`.

## Integrated batch 17 verification

Combined source and existing fixtures at a0de0a7 passed both dedicated builds, complete local CI steps 02–13, source guards with five inactive configurations unchanged, actual legacy line maps, per-unit parent/independent reviews and corrective combined-source review. The initial combined review found a matrix-mode cache surviving GL context recreation. Lifecycle, stream and target ownership were integrated together so the real COpenGL initialization fixes that cache contract before acceptance; the earlier validation cycle was superseded. Corrective review also found stale retained-context light defaults; the native initializer now explicitly uploads them. Full client linking exposed the missing native byte draw-flag import, now bound to actual storage initialized from original data0x34142c. The final full gate restarted after both repairs. Its ABI audit then exposed a dedicated-only placeholder still declared void(void); the native dedicated signature now matches the seven-parameter client provider, preserving its inactive link body. The complete stock/CoDx ABI step was rerun using the exact Release compile-command snapshots saved before fixture assertion adjustment; the failed original log is retained. Initial08 fixture imports and09/10 client-link failures remain in the superseded logs.

Existing GL fixtures now supply the restored state/math providers and required viewport setup. The dispatch fixture resolves the additional Classic Clear imports with its existing failure continuation, and depth now uses the actual draw-flag provider. Their scenarios and result assertions are retained; the obsolete native aborting matrix shim was replaced by the real provider. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. The all-file guard helper cannot preprocess the unchanged Apple-only dispatch restriction in its six synthetic configurations; the source-only guard run passes, and separate diagnostics restrict all12 preprocessing errors to that fixture in base/head. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This combined batch closes 108 color/math, asset, GPU timing, shader compiler and device state/lifecycle/stream/target entries. Full render-state dispatch, shader factory, registration/capability and indexed-draw follow-ups remain separately tracked. Original undefined-storage cases and native API/context/buffer mapping adaptations are explicit. Dedicated replay does not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
