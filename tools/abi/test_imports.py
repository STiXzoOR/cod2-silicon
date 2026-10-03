#!/usr/bin/env python3
"""Exercise the import check's distinctions without a licensed binary."""
import unittest

import imports


class ImportAuditTests(unittest.TestCase):
    def test_pointer_variable_and_pointer_array_loads_are_valid(self):
        site = {'access': imports.classification('value = *(const dvar_t **)')}
        self.assertFalse(imports.extra_pointer_load(site, ['pointer']))
        self.assertFalse(imports.extra_pointer_load(site, ['array:pointer']))

    def test_extra_struct_and_object_array_loads_fail(self):
        site = {'access': imports.classification('value = *(snd_local_t **)')}
        self.assertTrue(imports.extra_pointer_load(site, ['struct:snd_local_t']))
        self.assertTrue(imports.extra_pointer_load(site, ['array:struct']))
        self.assertTrue(imports.extra_pointer_load(site, ['array:range']))

    def test_object_address_and_scalar_field_reads_are_valid(self):
        address = {'access': imports.classification('value = (snd_local_t *)')}
        scalar = {'access': imports.classification('value = *(int *)')}
        self.assertFalse(imports.extra_pointer_load(address, ['struct:snd_local_t']))
        self.assertFalse(imports.extra_pointer_load(scalar, ['struct:snd_local_t']))

    def test_import_slot_load_is_distinct_from_variable_value_load(self):
        self.assertEqual(imports.classification('helper = *(FxHelper **)&'), 'import-slot-load')
        self.assertEqual(imports.classification('helper = *(FxHelper **)'), 'pointer-load')
        slot = {'access': 'import-slot-load'}
        self.assertEqual(imports.incorrect_import_load(slot, ['pointer']), 'missing-pointer-load')
        self.assertIsNone(imports.incorrect_import_load(slot, ['range']))
        self.assertIsNone(imports.incorrect_import_load(slot, ['array:range']))

    def test_comments_and_declarations_do_not_create_accesses(self):
        path = imports.ROOT / 'src/example.c'
        sites = list(imports.source_sites({path: 'extern void *imp_g_snd;\n'
                  '/* *(void **)imp_g_snd */\n'
                  'value = *(void **)imp_g_snd;\n'}))
        self.assertEqual(len(sites), 1)
        self.assertEqual(sites[0]['line'], 3)
        self.assertEqual(sites[0]['access'], 'pointer-load')

    def test_function_parameter_does_not_reference_generic_placeholder(self):
        stub = imports.ROOT / 'src/stubs/example.c'
        user = imports.ROOT / 'src/example.c'
        result = imports.placeholders({stub: 'char buf[64] = { 0 };\n',
                                      user: 'extern void f(char *buf);\nvoid g(void) { char buf[4]; }\n'})
        self.assertEqual(result[0]['uses'], [])


if __name__ == '__main__':
    unittest.main()
