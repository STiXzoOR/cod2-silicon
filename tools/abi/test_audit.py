#!/usr/bin/env python3
"""Exercise the ABI checker using real independent clang translation units."""
import json
from pathlib import Path
import tempfile
import unittest

import audit


class AuditTest(unittest.TestCase):
    def test_cross_translation_unit_contracts(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory(dir=root / 'tools/abi') as directory:
            directory = Path(directory)
            units = {
                'definition.c': '''
                    void scalar(float x) {}
                    void *handle(void *p) { return p; }
                    void log_message(const char *format, ...) {}
                    void arity(int a, int b) {}
                    typedef struct Result { int a, b; } Result;
                    Result aggregate(void) { return (Result){0}; }
                ''',
                'caller.c': '''
                    extern void scalar(int);
                    extern int handle(int);
                    extern void log_message(const char *, int);
                    extern void arity(int);
                    typedef struct Result { int a, b; } Result;
                    extern Result aggregate(void);
                ''',
                'unprototyped.c': '''
                    extern void scalar();
                    void caller(float x) { scalar(x); }
                ''',
            }
            cache = directory / 'cache'
            cache.mkdir()
            results = []
            for name, source in units.items():
                path = directory / name
                path.write_text(source)
                results.append(audit.summarize(
                    {'file': str(path), 'directory': str(directory),
                     'arguments': ['clang', '-std=c99', '-c', str(path)]},
                    root, cache, 'test'))
            findings = audit.mismatches([d for r in results for d in r['declarations']])
            self.assertEqual({f['name'] for f in findings},
                             {'scalar', 'handle', 'log_message', 'arity'})
            self.assertIn('variadic', next(f for f in findings if f['name'] == 'log_message')['reasons'])
            call, = results[2]['unprototyped_calls']
            self.assertEqual(call['args'], ['double'])

    def test_alias_tags_do_not_expand_recursively(self):
        aliases = {'Result': 'struct Result', 'Float': 'float', 'Ptr': 'Float *'}
        self.assertEqual(audit.canonical('Result', aliases), 'struct Result')
        self.assertEqual(audit.abi_type('struct Result', aliases), 'aggregate:struct Result')
        self.assertEqual(audit.abi_type('Ptr', aliases), 'pointer')
        self.assertEqual(audit.abi_type('Float', aliases), 'float')

    def test_sdk_prototypes_and_generic_vtable_storage(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory(dir=root / 'tools/abi') as directory:
            directory = Path(directory)
            cache = directory / 'cache'
            cache.mkdir()
            sources = {
                'sdk.c': '#include <stdlib.h>\n',
                'local.c': 'extern void *malloc(int);\n',
                'vtable.c': '''
                    typedef void (*fnptr_t)(void);
                    void entry(float value) {}
                    fnptr_t table[] = { (fnptr_t)entry };
                    void test(void) { (void)(fnptr_t)entry; }
                ''',
            }
            results = []
            for name, source in sources.items():
                path = directory / name
                path.write_text(source)
                results.append(audit.summarize(
                    {'file': str(path), 'directory': str(directory),
                     'arguments': ['clang', '-std=c99', '-c', str(path)]},
                    root, cache, 'sdk-test'))
            findings = audit.mismatches([d for r in results for d in r['declarations']])
            self.assertEqual({f['name'] for f in findings}, {'malloc'})
            casts = results[2]['function_casts']
            self.assertEqual([c['storage'] for c in casts], [True, False])

    def test_cpp_overloads_use_distinct_link_symbols(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory(dir=root / 'tools/abi') as directory:
            directory = Path(directory)
            path = directory / 'overloads.cpp'
            path.write_text('void f(int) {}\nvoid f(float) {}\n')
            cache = directory / 'cache'
            cache.mkdir()
            result = audit.summarize(
                {'file': str(path), 'directory': str(directory),
                 'arguments': ['clang++', '-c', str(path)]}, root, cache, 'cpp-test')
            self.assertEqual(audit.mismatches(result['declarations']), [])


if __name__ == '__main__':
    unittest.main()
