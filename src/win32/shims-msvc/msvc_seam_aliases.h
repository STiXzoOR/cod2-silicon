/* =============================================================================
 * msvc_seam_aliases.h -- engine-global seam aliases for the MSVC build
 *
 * The reconstruction reaches some engine globals through a second name (usually
 * an `<x>_ptr` byte-array base) that must resolve to the real symbol. The GNU /
 * web build binds these with linker `--defsym` (see the alias list in
 * src/blobs/literals.S); `/alternatename` is the exact MSVC equivalent. A
 * directive is a no-op unless `<lhs>` is actually referenced-but-undefined, so
 * this list holds ONLY seams that are live in the cl.exe link -- dead `--defsym`
 * entries from the GNU build are intentionally omitted.
 *
 * Hand-maintained (unlike the auto-generated msvc_alias_pragmas.h, which covers
 * the data-blob/stub alias pointers). Kept here rather than in the CMake link
 * flags so every /alternatename binding lives in source, in one place. x86 C
 * symbols carry one leading '_'.
 *
 * Included by msvc_link_glue.c so the directives reach the linker.
 * ===========================================================================*/
#ifdef _MSC_VER

/* game: level_locals + entity array bases (used as `<x>_ptr + n*stride`) */
#pragma comment(linker, "/alternatename:_level_ptr=_level")
#pragma comment(linker, "/alternatename:_g_entities_ptr=_g_entities")
#pragma comment(linker, "/alternatename:_g_time=_imp_level_bgs")
#pragma comment(linker, "/alternatename:_g_time_ptr=_imp_bgs")

/* server globals */
#pragma comment(linker, "/alternatename:_sv_ptr=_sv")
#pragma comment(linker, "/alternatename:_svs_ptr=_svs")

/* script VM */
#pragma comment(linker, "/alternatename:_scr_const_ptr=_scr_const")

/* renderer globals (rg = frontend data, vidConfig = limits) */
#pragma comment(linker, "/alternatename:_r_frontEndData_ptr=_rg")
#pragma comment(linker, "/alternatename:_r_limits_ptr=_vidConfig")

/* cgame UI globals; builtin-method table (address-suffixed blob symbol) */
#pragma comment(linker, "/alternatename:_cg_globUI=_legacyHacks")
#pragma comment(linker, "/alternatename:_methods=_methods_003138c0")

#endif /* _MSC_VER */
