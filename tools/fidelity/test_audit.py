"""Synthetic parser and regression tests; no game content."""
import importlib.util
from pathlib import Path
import sys
import unittest

SPEC = importlib.util.spec_from_file_location('fidelity', Path(__file__).with_name('audit.py'))
audit = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = audit
if SPEC.loader and SPEC.origin and Path(SPEC.origin).exists():
    SPEC.loader.exec_module(audit)


class AuditTests(unittest.TestCase):
    def test_stabs_inventory_keeps_local_functions_and_extent(self):
        rows = [('/private/source/PC/bgame/example.cpp', 0x64, 0, 0, 0),
                ('_ZL4Ticki:f(0,1)', 0x24, 1, 7, 0x100),
                ('', 0x24, 0, 0, 36)]
        funcs = audit.stabs_functions(rows)
        self.assertEqual((funcs[0]['name'], funcs[0]['size']), ('Tick', 36))
        self.assertEqual(funcs[0]['source'], '/private/source/PC/bgame/example.cpp')

    def test_call_resolution_maps_import_stubs_and_numeric_targets(self):
        refs = [{'name': 'Tick', 'symbol': '__Z4Tickv', 'address': 0x100, 'size': 16}]
        code = '00000100\tcalll\tL_memset$stub\n00000105\tcalll\t0x200\n0000010a\tje\t0x10f\n'
        result = audit.disassembly_features(code, refs, {0x200: 'Other'})
        self.assertEqual(result[0x100]['callees'], ['Other', 'memset'])
        self.assertEqual(result[0x100]['branches'], 1)

    def test_ast_definition_records_only_callee_not_function_argument(self):
        ref = lambda name: {'kind': 'DeclRefExpr', 'referencedDecl': {'kind': 'FunctionDecl', 'name': name}}
        tree = {'kind': 'TranslationUnitDecl', 'inner': [
            {'kind': 'FunctionDecl', 'name': 'Tick', 'loc': {'file': '/repo/src/test.c', 'line': 4},
             'inner': [{'kind': 'CompoundStmt', 'inner': [
                 {},
                 {'kind': 'CallExpr', 'inner': [ref('Dispatch'), ref('Callback')]},
                 {'kind': 'FloatingLiteral', 'value': '0.5'},
                 {'kind': 'StringLiteral', 'value': '"example_dvar"'}]}]}]}
        funcs = audit.ast_features(tree, Path('/repo'), Path('/repo/src/test.c'), '')
        self.assertEqual(funcs[0]['callees'], ['Dispatch'])
        self.assertEqual(funcs[0]['floats'], [0.5])
        self.assertEqual(funcs[0]['strings'], ['example_dvar'])

    def test_generic_missing_calls_and_shape_flag_simplification(self):
        ref = {'callees': ['A', 'B'], 'branches': 20, 'instructions': 200, 'floats': [], 'strings': []}
        native = {'callees': ['A'], 'branches': 1, 'nodes': 8, 'floats': [], 'strings': [], 'stubs': []}
        finding = audit.compare(ref, native, {})
        self.assertIn('B', finding['missing_callees'])
        self.assertGreater(finding['score'], 0)

    def test_inline_expansion_avoids_false_missing_callee(self):
        helper = {'callees': ['Leaf'], 'nodes': 6}
        ref = {'callees': ['Leaf'], 'branches': 0, 'instructions': 5, 'floats': [], 'strings': []}
        native = {'callees': ['Helper'], 'branches': 0, 'nodes': 10, 'floats': [], 'strings': [], 'stubs': []}
        self.assertEqual(audit.compare(ref, native, {'Helper': helper})['missing_callees'], [])

    def test_stub_patterns_ignore_strings_and_honor_void_argument_casts(self):
        self.assertEqual(audit.stub_patterns('{ (void)x; output->value = 0.0f; }'), ['zero-output'])
        self.assertEqual(audit.stub_patterns('{ return 0; }'), ['constant-return'])
        self.assertEqual(audit.stub_patterns('{ }'), ['empty-body'])
        self.assertEqual(audit.stub_patterns('{ *out = 0; }'), ['zero-output'])
        self.assertEqual(audit.stub_patterns('{ memset(out, 0, size); }'), ['zero-output'])
        self.assertEqual(audit.stub_patterns('{ Log("TODO stub"); Work(); }'), [])
        self.assertIn('comment:approximate', audit.stub_patterns('{ /* approximate */ Work(); }'))

    def test_stub_gate_counts_duplicates_and_ignores_line_moves(self):
        old = [{'file': 'src/a.c', 'name': 'Tick', 'patterns': ['empty-body']}]
        self.assertEqual(audit.new_stubs(old, old), [])
        self.assertEqual(len(audit.new_stubs(old + old, old)), 1)

    def test_reference_float_dvar_recovers_register_loaded_literal(self):
        import struct
        raw = int.from_bytes(struct.pack('<f', 0.5), 'little')
        ins = [('movl', '$0x500, (%esp)'), ('movl', '$' + hex(raw) + ', 0x4(%esp)'),
               ('movl', '$0, 0x8(%esp)'), ('movl', '$0x3f800000, 0xc(%esp)'),
               ('movl', '$0x20, 0x10(%esp)'), ('calll', '__Z18Dvar_RegisterFloatPKcfff t')]
        calls = audit.reference_dvars(ins, lambda addr: ('string', 'volume') if addr == 0x500 else None)
        self.assertEqual(calls[0]['arguments'], ['volume', 0.5, 0.0, 1.0, 32])

    def test_branch_invalidates_dvar_arguments_instead_of_guessing(self):
        ins = [('movl', '$0x500, (%esp)'), ('jne', '0x90'),
               ('calll', '__Z17Dvar_RegisterBoolPKcit')]
        self.assertEqual(audit.reference_dvars(ins, lambda addr: ('string', 'example')), [])

    def test_cpp_member_names_match_c_reconstruction(self):
        self.assertEqual(audit.name_of('__ZN7Example4TickEv'), 'Example_Tick')
        self.assertEqual(audit.name_of('__ZN7ExampleC1Ev'), 'Example_Example')
        self.assertEqual(audit.name_of('__ZNK7Example4TickEv'), 'Example_Tick')

    def test_native_only_helper_is_attributed_to_mismatching_reference_caller(self):
        functions = [{'name': 'Move', 'callees': ['Helper'], 'file': 'src/a.c'},
                     {'name': 'Helper', 'callees': [], 'file': 'src/a.c'}]
        rows = [{'reference': {'name': 'Move', 'address': 0x100}, 'native': functions[0],
                 'comparison': {'score': 16}}]
        helpers = audit.native_only_context(rows, functions)
        self.assertEqual(helpers[0]['name'], 'Helper')
        self.assertEqual(helpers[0]['ancestor_signals'][0]['address'], 0x100)

    def test_large_extracted_helper_is_not_an_omitted_call(self):
        helper = {'callees': ['Leaf'], 'nodes': 400}
        ref = {'callees': ['Leaf'], 'branches': 20, 'instructions': 200, 'floats': [], 'strings': []}
        native = {'callees': ['Helper'], 'branches': 0, 'nodes': 10, 'floats': [], 'strings': [], 'stubs': []}
        result = audit.compare(ref, native, {'Helper': helper})
        self.assertEqual(result['missing_callees'], [])
        self.assertLess(result['score'], 8)

    def test_empty_body_that_is_empty_in_reference_does_not_flag(self):
        ref = {'callees': [], 'branches': 0, 'instructions': 4, 'floats': [], 'strings': []}
        native = {'callees': [], 'branches': 0, 'nodes': 1, 'floats': [], 'strings': [], 'stubs': ['empty-body']}
        self.assertEqual(audit.compare(ref, native, {})['score'], 0)

    def test_extracted_implementation_and_abi_bridge_share_reference_identity(self):
        ref = {'callees': ['Render', 'Decode'], 'branches': 20, 'instructions': 100,
               'floats': [0.5], 'strings': ['key']}
        helper = {'name': 'Tick_impl', 'callees': ['Render_impl', 'DecodeReg'],
                  'branches': 20, 'floats': [0.5], 'strings': ['key']}
        native = {'name': 'Tick', 'callees': ['Tick_impl'], 'branches': 0,
                  'floats': [], 'strings': [], 'stubs': []}
        result = audit.compare(ref, native, {'Tick_impl': helper, 'Decode': {}, 'DecodeReg': {}})
        self.assertEqual(result['missing_callees'], [])
        self.assertEqual(result['missing_strings'], [])
        self.assertEqual(result['missing_floats'], [])
        self.assertEqual(result['score'], 0)

    def test_encoded_template_callee_names_normalize_without_game_function_list(self):
        ref = {'callees': ['Select'], 'branches': 0, 'instructions': 8, 'floats': [], 'strings': []}
        native = {'callees': ['Z6SelectIiEvT_'], 'branches': 0, 'floats': [], 'strings': [], 'stubs': []}
        self.assertEqual(audit.compare(ref, native, {})['missing_callees'], [])

    def test_dvars_include_native_only_registration_helpers(self):
        funcs = [{'name': 'NewInit', 'file': 'src/new.c', 'dvars': [
            {'type': 'Bool_mac', 'arguments': ['option', 1, 128]}]}]
        result = audit.dvar_report([], funcs)
        self.assertEqual(result['native_count'], 1)
        self.assertEqual(result['native_registrations']['option']['type'], 'Bool')
        funcs[0]['dvars'] = [{'type': 'Int', 'arguments': ['option', 0, 0x80000000, 0x7fffffff, 128]}]
        result = audit.dvar_report([], funcs)
        self.assertEqual(result['native_registrations']['option']['arguments'][2], -2147483648)

    def test_unmodeled_arithmetic_cannot_create_false_dvar_default(self):
        ins = [('movl', '$0x500, (%esp)'), ('movl', '$10, %esi'),
               ('addl', '$5, %esi'), ('movl', '%esi, 0x4(%esp)'),
               ('movl', '$0, 0x8(%esp)'), ('movl', '$20, 0xc(%esp)'),
               ('movl', '$128, 0x10(%esp)'), ('calll', '__Z16Dvar_RegisterIntPKciiit')]
        calls = audit.reference_dvars(ins, lambda addr: ('string', 'example') if addr == 0x500 else None)
        self.assertIsNone(calls[0]['arguments'][1])

    def test_string_summary_never_persists_embedded_code(self):
        self.assertEqual(audit.string_summary('player_speed'), 'player_speed')
        self.assertEqual(audit.string_summary('@MP_PLAY'), '@MP_PLAY')
        value = audit.string_summary('void main() {\n return; }')
        self.assertTrue(value.startswith('sha256:'))
        self.assertNotIn('return', value)

    def test_real_compile_database_extraction_and_cache(self):
        import json
        import tempfile
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as tmp:
            directory = Path(tmp)
            source = directory / 'synthetic.c'
            source.write_text('extern void Leaf(void); void Tick(void) { Leaf(); }')
            database = directory / 'commands.json'
            database.write_text(json.dumps([{'file': str(source), 'directory': tmp,
                'arguments': ['clang', '-c', str(source), '-o', str(directory / 'cod2_macos.dir/Tick.o')]}]))
            funcs = audit.native_inventory(database, directory / 'cache', 1)
            self.assertEqual(funcs[0]['callees'], ['Leaf'])
            self.assertEqual(audit.native_inventory(database, directory / 'cache', 1), funcs)

    def test_overloaded_definitions_are_explicitly_ambiguous(self):
        refs = [{'name': 'Tick', 'source': 'original.cpp', 'callees': [], 'branches': 0,
                 'instructions': 5, 'floats': [], 'strings': []}]
        native = [{'name': 'Tick', 'file': 'src/a.c'}, {'name': 'Tick', 'file': 'src/b.c'}]
        self.assertEqual(audit.build_report(refs, native)[0]['status'], 'ambiguous')

    def test_literal_reader_bounds_and_type(self):
        import struct
        sections = [{'name': '__literal4', 'address': 0x400, 'size': 4, 'offset': 0}]
        self.assertEqual(audit.literal_at(struct.pack('<f', 0.25), sections, 0x400), ('float', 0.25))
        self.assertIsNone(audit.literal_at(b'xxxx', sections, 0x404))
        sections[0].update(name='__const', size=8)
        self.assertEqual(audit.literal_at(struct.pack('<d', 0.125), sections, 0x400, 8), ('float', 0.125))

    def test_dvar_stack_call_decodes_signed_int_and_flags(self):
        ins = [('movl', '$0x500, (%esp)'), ('movl', '$0xffffffff, 0x4(%esp)'),
               ('movl', '$0xfffffffe, 0x8(%esp)'), ('movl', '$0xa, 0xc(%esp)'),
               ('movl', '$0x80, 0x10(%esp)'), ('calll', '__Z16Dvar_RegisterIntPKciii t')]
        calls = audit.reference_dvars(ins, lambda addr: ('string', 'example') if addr == 0x500 else None)
        self.assertEqual(calls[0]['arguments'], ['example', -1, -2, 10, 128])


if __name__ == '__main__':
    unittest.main()
