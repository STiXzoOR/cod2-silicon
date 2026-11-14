#!/usr/bin/env python3
"""Regenerate independent Xiph PCM under the Windows 15-bit CRT contract."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

from regenerate import SOURCE_SHA256
from run import BACKGROUND, FIXTURES, observe

CASES = {(1, 0, False), (1, 0, True), (8, 0, True)}
CRT_HEADER = '''#include <stdlib.h>
#undef RAND_MAX
#define RAND_MAX 32767
int Reference_CrtRand(void);
#define rand Reference_CrtRand
'''
CRT_SOURCE = '''#include <stdint.h>
static uint32_t reference_seed = 1;
void Com_CrtSrand(unsigned int seed) { reference_seed = seed; }
int Reference_CrtRand(void)
{
    reference_seed = reference_seed * UINT32_C(214013) + UINT32_C(2531011);
    return (int)((reference_seed >> 16) & UINT32_C(32767));
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-archive', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if hashlib.sha256(args.source_archive.read_bytes()).hexdigest() != SOURCE_SHA256:
        parser.error('Expected the pinned official Xiph Speex 1.0.4 archive')
    fixture = json.loads(FIXTURES.read_text())
    with tempfile.TemporaryDirectory(prefix='cod2-voice-windows-crt-') as directory:
        temporary = Path(directory)
        with tarfile.open(args.source_archive) as archive:
            archive.extractall(temporary)
        source = temporary / 'speex-1.0.4'
        header, crt = temporary / 'crt.h', temporary / 'crt.c'
        header.write_text(CRT_HEADER)
        crt.write_text(CRT_SOURCE)
        flags = ['clang', '-arch', 'arm64', '-O3', '-ffp-contract=off',
                 '-fno-strict-aliasing', '-I' + str(source / 'include'),
                 '-I' + str(source / 'libspeex')]
        sources = sorted(p for p in (source / 'libspeex').glob('*.c')
                         if not p.name.startswith('testenc'))
        library, probe = temporary / 'upstream.dylib', temporary / 'probe'
        subprocess.run(BACKGROUND + flags + ['-dynamiclib', '-include', str(header)] +
                       list(map(str, sources)) + [str(crt), '-o', str(library)],
                       check=True, capture_output=True, text=True)
        subprocess.run(BACKGROUND + flags + [str(FIXTURES.with_name('codec_probe.c')),
                       '-o', str(probe)], check=True, capture_output=True, text=True)
        decode = []
        for case in fixture['decode']:
            if (case['mode'], case['enhancement'], case['loss']) not in CASES:
                continue
            metadata, expected = observe(probe, library, 'decode', case['enhancement'], case['sequence'])
            assert metadata == dict(version='speex-1.0.4', bitstream_version=4)
            decode.append(dict(mode=case['mode'], enhancement=case['enhancement'],
                               loss=case['loss'], expected=expected))
    result = dict(provenance=dict(
        decoder='Xiph Speex 1.0.4 official source; source files unchanged',
        url='https://downloads.xiph.org/releases/speex/speex-1.0.4.tar.gz',
        source_archive_sha256=SOURCE_SHA256,
        fixture_sha256=hashlib.sha256(FIXTURES.read_bytes()).hexdigest(),
        compile='clang -arch arm64 -O3 -ffp-contract=off -fno-strict-aliasing',
        decoder_seed=17, rand_max=32767,
        crt='uint32 state = state * 214013 + 2531011; return (state >> 16) & 32767',
        crt_validation='4096 seed-17 values matched actual Windows 1.3 CRT machine code; the same PCM was independently reproduced with that recorded draw table',
        header_sha256=hashlib.sha256(CRT_HEADER.encode()).hexdigest(),
        bridge_sha256=hashlib.sha256(CRT_SOURCE.encode()).hexdigest(),
        note='Independent upstream PCM only; no engine or original-game decoded PCM.'), decode=decode)
    args.output.write_text(json.dumps(result, separators=(',', ':')) + '\n')
    print(f'Windows CRT profile written to {args.output}')


if __name__ == '__main__':
    main()
