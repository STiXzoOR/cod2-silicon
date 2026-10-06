"""Synthetic native trace guard regressions; no engine or game data needed."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('trace_guards', ROOT / 'tools/check_trace_guards.py')
guards = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guards)


class TraceGuardTests(unittest.TestCase):
    def check(self, code, count):
        self.assertEqual(len(guards.scan_source(code)), count)

    def test_unguarded_tags_and_files(self):
        self.check('void f() { Com_Printf("[cnx] hello"); fopen("x64_ui_trace.txt", "w"); }', 2)
        self.check('void f() { Com_Printf("[screenshot-" "trace] hi"); fopen("x64_" "ui_trace.txt", "w"); }', 2)

    def test_trace_if_and_early_return(self):
        self.check('void f() { if (COD2_NATIVE_TRACE_ENABLED() && n < 3) { Com_Printf("[cnx] hello"); } }', 0)
        self.check('void f() { if (!COD2_NATIVE_TRACE_ENABLED()) return; fopen("cod2_heapdiag.txt", "w"); }', 0)
        self.check('void f() { if (!COD2_DEBUG_ENV("TEXTCMD_DBG") || !font) return; Com_Printf("[textcmd] hi"); }', 0)
        self.check('void f() { COD2_NATIVE_TRACE_ONLY(Com_Printf("[cnx] hi");) }', 0)

    def test_guard_does_not_cover_sibling_or_next_function(self):
        self.check('void f() { if (COD2_NATIVE_TRACE_ENABLED()) { work(); } Com_Printf("[cnx] hi"); }', 1)
        self.check('void f() { if (!COD2_NATIVE_TRACE_ENABLED()) return; work(); } void g() { Com_Printf("[cnx] hi"); }', 1)

    def test_negative_and_or_conditions_are_not_guards(self):
        self.check('void f() { if (!COD2_NATIVE_TRACE_ENABLED()) Com_Printf("[cnx] hi"); }', 1)
        self.check('void f() { if (COD2_NATIVE_TRACE_ENABLED() || ready) Com_Printf("[cnx] hi"); }', 1)
        self.check('void f() { if (COD2_NATIVE_TRACE_ENABLED() == 0) Com_Printf("[cnx] hi"); }', 1)
        self.check('void f() { if (COD2_NATIVE_TRACE_ENABLED() != 0) Com_Printf("[cnx] hi"); }', 0)

    def test_failed_guard_does_not_protect_else(self):
        self.check('void f() { if (COD2_NATIVE_TRACE_ENABLED()) work(); else Com_Printf("[cnx] hi"); }', 1)

    def test_unresolved_native_features_do_not_enable_windows_branch(self):
        self.check('#if defined(_WIN32) && defined(OPTIONAL_FLAG)\nvoid f() { Com_Printf("[win-trace] hi"); }\n#endif\n', 0)

    def test_unrelated_environment_variable_does_not_count(self):
        self.check('void f() { if (getenv("MTRACE")) Com_Printf("[menu-trace] hi"); }', 1)

    def test_reconstruction_probes_compile_away(self):
        self.check('void f() { COD2_DEBUG_ONLY(Com_Printf("[ckpt] hi");) if (COD2_DEBUG_ENV("MTRACE")) Com_Printf("[menu-trace] hi"); }', 0)

    def test_comments_are_ignored_and_multiline_calls_are_checked(self):
        self.check('/* Com_Printf("[cnx] hi"); */ void f() { // fopen("x64_ui_trace.txt", "w");\n ri.Printf(0,\n "[screenshot-trace] hi"); }', 1)
        self.check('void f() { Com_Printf("usage: dir [extension]"); }', 0)

    def test_native_branch_does_not_implicitly_guard_trace(self):
        self.check('#if defined(COD2_X64)\nvoid f() { Com_Printf("[cnx] hi"); }\n#endif\n', 1)

    def test_legacy_and_nonrelease_probes_are_ignored(self):
        self.check('#if defined(COD2_X64)\nvoid f() {}\n#else\nvoid f() { Com_Printf("[cnx] hi"); }\n#endif\n#if defined(_DEBUG)\nvoid g() { fopen("cod2_heapdiag.txt", "w"); }\n#endif\n', 0)

    def test_unknown_preprocessor_flags_are_conservative(self):
        self.check('#ifdef SOMETHING\nvoid f() { Com_Printf("[cnx] hi"); }\n#endif\n', 1)


if __name__ == '__main__':
    unittest.main()
