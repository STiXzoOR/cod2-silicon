"""Synthetic registration fixtures; expected facts are hand-derived."""
import copy
import tempfile
import unittest
from pathlib import Path

import catalog


class CoverageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'src').mkdir()
        (self.root / 'src/fixture.c').write_text('''
void Init(void) {
    Dvar_RegisterBool("safe", 1, 0x1001);
#if defined(COD2_CODX) && COD2_CODX
    Dvar_RegisterInt("rate", 25000, 1000, 25000, 0x1003);
#else
    Dvar_RegisterInt("rate", 5000, 1000, 25000, 0x1003);
#endif
    ri.Dvar_RegisterFloat("protected", 1.0f, 0.0f, 4.0f, 0x2080);
    Cmd_AddCommand("exec", Exec);
}
''')
        self.document = {
            'settings': [
                {'name': 'safe', 'availability': 'registered', 'variants': {
                    mode: {'sites': [{'type': 'bool', 'default': {'status': 'literal', 'value': True},
                                     'flags': 4097}]} for mode in ['stock', 'cod2x']}},
                {'name': 'rate', 'availability': 'registered', 'variants': {
                    'stock': {'sites': [{'type': 'int', 'default': {'status': 'literal', 'value': 5000}, 'flags': 4099, 'minimum': {'status': 'literal', 'value': 1000}, 'maximum': {'status': 'literal', 'value': 25000}}]},
                    'cod2x': {'sites': [{'type': 'int', 'default': {'status': 'literal', 'value': 25000}, 'flags': 4099, 'minimum': {'status': 'literal', 'value': 1000}, 'maximum': {'status': 'literal', 'value': 25000}}]}}},
                {'name': 'protected', 'availability': 'registered', 'variants': {
                    mode: {'sites': [{'type': 'float', 'default': {'status': 'literal', 'value': 1.0}, 'flags': 8320, 'minimum': {'status': 'literal', 'value': 0.0}, 'maximum': {'status': 'literal', 'value': 4.0}}]}
                    for mode in ['stock', 'cod2x']}},
            ],
            'commands': [{'name': 'exec', 'availability': 'registered', 'variants': {
                mode: {'sites': [{'name': 'exec', 'path': 'src/fixture.c', 'line': 10,
                                  'mechanism': 'Cmd_AddCommand'}]} for mode in ['stock', 'cod2x']}}],
        }

    def errors(self, document=None, mode='stock'):
        return catalog.coverage_errors(document or self.document, catalog.extract_source(self.root, mode), mode)

    def test_missing_setting_and_command_fail(self):
        doc = copy.deepcopy(self.document)
        doc['settings'].pop(0)
        doc['commands'] = []
        errors = self.errors(doc)
        self.assertTrue(any('missing setting safe' in e for e in errors), errors)
        self.assertTrue(any('missing command exec' in e for e in errors), errors)

    def test_extra_claimed_registration_fails(self):
        doc = copy.deepcopy(self.document)
        doc['settings'].append({'name': 'invented', 'availability': 'registered', 'variants': {'stock': {}}})
        doc['commands'].append({'name': 'alias', 'availability': 'registered', 'variants': {'stock': {}}})
        errors = self.errors(doc)
        self.assertTrue(any('extra setting invented' in e for e in errors), errors)
        self.assertTrue(any('extra command alias' in e for e in errors), errors)

    def test_missing_all_command_evidence_fails(self):
        doc = copy.deepcopy(self.document)
        doc['commands'][0]['variants']['stock'] = {}
        self.assertTrue(any('sites mismatch exec' in e for e in self.errors(doc)))

    def test_type_and_default_mismatch_fail(self):
        doc = copy.deepcopy(self.document)
        doc['settings'][0]['variants']['stock']['sites'][0]['type'] = 'int'
        doc['settings'][1]['variants']['stock']['sites'][0]['default']['value'] = 125
        errors = self.errors(doc)
        self.assertTrue(any('type mismatch safe' in e for e in errors), errors)
        self.assertTrue(any('default mismatch rate' in e for e in errors), errors)

    def test_modes_and_indirect_calls_are_covered(self):
        self.assertEqual(self.errors(), [])
        self.assertEqual(self.errors(mode='cod2x'), [])

    def test_reference_only_entries_do_not_claim_registration(self):
        doc = copy.deepcopy(self.document)
        doc['settings'].append({'name': 'scr_example', 'availability': 'scriptOrMod', 'variants': {}})
        self.assertEqual(self.errors(doc), [])
        doc['settings'][0]['availability'] = 'referenceOnly'
        self.assertTrue(any('missing setting safe' in e for e in self.errors(doc)))

    def test_unreviewed_indirect_name_fails_closed(self):
        with (self.root / 'src/fixture.c').open('a') as f:
            f.write('void Other(void) { Dvar_RegisterBool(unknownName, 0, 1); }\n')
        with self.assertRaisesRegex(catalog.CatalogError, 'unresolved registration name'):
            catalog.extract_source(self.root, 'stock')

    def test_tables_and_dynamic_values_remain_explicit(self):
        (self.root / 'src/fixture.c').write_text('''
static const char *choices[] = {"fast", "slow", 0};
static const char *names[] = {"offset_a", "offset_b"};
static const consoleCommand_t commandsList[] = {{"mp_QuickMessage", Quick}, {0, 0}};
void Init(void) {
    for (int i = 0; i < 2; i++) Dvar_RegisterVec3(names[i], 0, 0, 0, -1, 1, 0x1080);
    Dvar_RegisterEnum("choice", choices, 1, 1);
    Dvar_RegisterString("home", Sys_DefaultHomePath(), 0x1010);
    for (int i = 0; i < 1; i++) CL_AddCgameCommand(commandsList[i].cmd);
}
''')
        facts = catalog.extract_source(self.root, 'stock')
        self.assertEqual(set(facts['settings']), {'offset_a', 'offset_b', 'choice', 'home'})
        self.assertEqual(facts['settings']['choice'][0]['enumValues']['value'], ['fast', 'slow'])
        self.assertEqual(facts['settings']['home'][0]['default']['status'], 'dynamic')
        self.assertEqual(set(facts['commands']), {'mp_quickmessage'})

    def test_runtime_flags_are_explicit_and_conservatively_protected(self):
        (self.root / 'src/fixture.c').write_text('void Init(void) { Dvar_RegisterBool("policy", 0, dedicated ? 0x1008 : 0x18); }')
        site = catalog.extract_source(self.root, 'stock')['settings']['policy'][0]
        self.assertIsNone(site['flags'])
        self.assertEqual(site['flagCandidates'], [4104, 24])
        self.assertEqual(site['flagsExpression'], 'dedicated ? 0x1008 : 0x18')

    def test_signed_bounds_and_float_suffixes_are_not_dynamic(self):
        (self.root / 'src/fixture.c').write_text('void Init(void) { Dvar_RegisterFloat("f", 1e-8f, -FLT_MAX, FLT_MAX, 1); Dvar_RegisterInt("i", 0, (int)0x80000000, 0x7fffffff, 1); }')
        facts = catalog.extract_source(self.root, 'stock')['settings']
        self.assertEqual(facts['i'][0]['minimum']['value'], -2147483648)
        self.assertEqual(facts['i'][0]['maximum']['value'], 2147483647)
        self.assertEqual(facts['f'][0]['default']['value'], 1e-8)
        self.assertEqual(facts['f'][0]['maximum']['status'], 'literal')

    def test_game_dispatch_commands_are_distinct_from_registration(self):
        (self.root / 'src/PC/game_mp').mkdir(parents=True)
        (self.root / 'src/PC/game_mp/g_svcmds_mp.c').write_text('qboolean ConsoleCommand(void) { if (I_stricmp(cmd, "addip") == 0) return 1; return 0; }')
        facts = catalog.extract_source(self.root, 'stock')
        self.assertEqual(facts['dispatchCommands']['addip'][0]['mechanism'], 'ConsoleCommand:dispatch')
        self.assertNotIn('addip', facts['commands'])

    def test_same_default_is_not_changed_just_because_source_lines_differ(self):
        copy_path = self.root / 'copy.json'
        copy_path.write_text('{"settings": {}, "commands": {}}')
        doc = catalog.build_catalog(self.root, 'fixture', copy_path)
        safe = next(e for e in doc['settings'] if e['name'] == 'safe')
        self.assertEqual(safe['cod2x'], 'unchanged')
        protected = next(e for e in doc['settings'] if e['name'] == 'protected')
        self.assertFalse(protected['variants']['stock']['editable'])

    def test_copy_cannot_overwrite_source_facts_or_claim_unknown_names(self):
        copy_path = self.root / 'copy.json'
        copy_path.write_text('{"settings": {"safe": {"variants": {}}}}')
        with self.assertRaisesRegex(catalog.CatalogError, 'authored copy cannot replace'):
            catalog.build_catalog(self.root, 'fixture', copy_path)
        copy_path.write_text('{"settings": {"invented": {"title": "Oops"}}}')
        with self.assertRaisesRegex(catalog.CatalogError, 'copy for unregistered'):
            catalog.build_catalog(self.root, 'fixture', copy_path)

    def test_supplemental_cannot_shadow_an_actual_registration(self):
        copy_path = self.root / 'copy.json'
        copy_path.write_text('{"supplemental": {"settings": [{"name": "safe", "availability": "referenceOnly"}]}}')
        with self.assertRaisesRegex(catalog.CatalogError, 'supplemental shadows registration'):
            catalog.build_catalog(self.root, 'fixture', copy_path)

    def test_coverage_ignores_line_shifts_but_not_lost_command_sites(self):
        copy_path = self.root / 'copy.json'
        copy_path.write_text('{}')
        doc = catalog.build_catalog(self.root, 'fixture', copy_path)
        with (self.root / 'src/fixture.c').open('r+') as f:
            contents = f.read(); f.seek(0); f.write('\n\n' + contents)
        self.assertEqual(self.errors(doc), [])
        doc['commands'][0]['variants']['stock']['sites'] = []
        self.assertTrue(any('sites mismatch exec' in e for e in self.errors(doc)))

    def test_unreviewed_source_selection_rule_fails_closed(self):
        (self.root / 'cmake').mkdir()
        (self.root / 'cmake/macos-arm64.cmake').write_text('set(MACOS_C ${MACOS_PC_C} ${MACOS_MAC_C} ${MACOS_STUBS_C} ${MACOS_ROOT_C})\ntarget_sources(cod2_macos PRIVATE ${UNKNOWN_SOURCES})\n')
        with self.assertRaisesRegex(catalog.CatalogError, 'unreviewed source selection'):
            catalog.extract_source(self.root, 'stock')

    def test_unreviewed_registration_constructor_fails_closed(self):
        with (self.root / 'src/fixture.c').open('a') as f:
            f.write('void Other(void) { Dvar_RegisterLong("newtype", 1, 0, 10, 1); }\n')
        with self.assertRaisesRegex(catalog.CatalogError, 'unreviewed registration constructor'):
            catalog.extract_source(self.root, 'stock')

    def test_included_implementation_registrations_and_macros_are_covered(self):
        (self.root / 'src/impl.h').write_text('#ifndef IMPL_H\n#define IMPL_H\n#define UDP_DEFAULT 4\nstatic void HeaderInit(void) { Dvar_RegisterInt("udp", UDP_DEFAULT, 1, 64, 0x1000); Cmd_AddCommand("udp_status", Status); }\n#endif\n')
        (self.root / 'src/fixture.c').write_text('#include "impl.h"\nvoid Init(void) { HeaderInit(); }\n')
        facts = catalog.extract_source(self.root, 'stock')
        self.assertEqual(facts['settings']['udp'][0]['default']['value'], 4)
        self.assertEqual(facts['settings']['udp'][0]['path'], 'src/impl.h')
        self.assertEqual(facts['settings']['udp'][0]['line'], 4)
        self.assertEqual(set(facts['commands']), {'udp_status'})

    def test_nested_headers_do_not_duplicate_sites_between_translation_units(self):
        (self.root / 'src/defs.h').write_text('#define SHARED_DEFAULT 8\n')
        (self.root / 'src/impl.h').write_text('#include "defs.h"\nstatic void HeaderInit(void) { Dvar_RegisterBool("shared", 1, SHARED_DEFAULT); }\n')
        (self.root / 'src/fixture.c').write_text('#include "impl.h"\n')
        (self.root / 'src/second.c').write_text('#include "impl.h"\n')
        facts = catalog.extract_source(self.root, 'stock')
        self.assertEqual(len(facts['settings']['shared']), 1)
        self.assertEqual(facts['settings']['shared'][0]['flags'], 8)
        self.assertEqual(facts['settings']['shared'][0]['line'], 2)

    def test_constant_name_index_does_not_claim_the_rest_of_the_table(self):
        (self.root / 'src/fixture.c').write_text('static const char *names[] = {"present", "never_registered"}; void Init(void) { Dvar_RegisterBool(names[0], 0, 1); }')
        facts = catalog.extract_source(self.root, 'stock')
        self.assertEqual(set(facts['settings']), {'present'})

    def test_integer_division_uses_c_semantics(self):
        (self.root / 'src/fixture.c').write_text('void Init(void) { Dvar_RegisterFloat("fraction", 5/2, 0, 10, 1); }')
        site = catalog.extract_source(self.root, 'stock')['settings']['fraction'][0]
        self.assertEqual(site['default']['value'], 2)

    def test_unknown_native_glob_definition_fails_closed(self):
        (self.root / 'cmake').mkdir()
        (self.root / 'cmake/macos-arm64.cmake').write_text('file(GLOB_RECURSE MACOS_PC_C "${COD2_SRC_DIR}/PC/*.c" "${COD2_SRC_DIR}/added/*.c")\nset(MACOS_C ${MACOS_PC_C} ${MACOS_MAC_C} ${MACOS_STUBS_C} ${MACOS_ROOT_C})\n')
        with self.assertRaisesRegex(catalog.CatalogError, 'unreviewed source selection'):
            catalog.extract_source(self.root, 'stock')

    def test_include_cycle_cache_does_not_hide_a_later_enabled_registration(self):
        (self.root / 'src/a.h').write_text('#ifndef A_H\n#define A_H\n#include "b.h"\n#ifndef DISABLE_A\nstatic void A(void) { Dvar_RegisterBool("a", 0, 1); }\n#endif\n#endif\n')
        (self.root / 'src/b.h').write_text('#ifndef B_H\n#define B_H\n#include "a.h"\nstatic void B(void) { Dvar_RegisterBool("b", 0, 1); }\n#endif\n')
        (self.root / 'src/fixture.c').write_text('#define DISABLE_A 1\n#include "a.h"\n')
        (self.root / 'src/second.c').write_text('#include "b.h"\n')
        self.assertEqual(set(catalog.extract_source(self.root, 'stock')['settings']), {'a', 'b'})

    def test_system_include_and_blank_lines_preserve_registration_line(self):
        (self.root / 'src/fixture.c').write_text('\n\n#include <stdio.h>\n\nvoid Init(void) { Dvar_RegisterBool("line", 0, 1); }\n')
        self.assertEqual(catalog.extract_source(self.root, 'stock')['settings']['line'][0]['line'], 5)


if __name__ == '__main__':
    unittest.main()
