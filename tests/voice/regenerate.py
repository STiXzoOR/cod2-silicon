#!/usr/bin/env python3
"""Rebuild synthetic fixtures using external independent libraries, never the engine."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

from run import BACKGROUND, observe

SOURCE_SHA256 = 'd0708ab9d1a6ae6aa8f59cf7b8c37c1cf03fc2c2fe20401a03b08c57c9898630'
MODERN_SHA256 = '489035822ba8ab1484b6a144a99bdc84a7994a4ffe87baafdf8c14ecb77bd7f3'
INPUT = 'codec_probe.c InputFrame; 8 frames; 160 samples; integer triangle + saw + LCG noise'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-archive', required=True, type=Path, help='Official Speex 1.0.4 tar.gz')
    parser.add_argument('--modern-library', required=True, type=Path, help='Audited x86_64 Speex 1.2.1 dylib')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    for path, expected in [(args.source_archive, SOURCE_SHA256), (args.modern_library, MODERN_SHA256)]:
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            parser.error(f'Unrecognized independent input: {path}')
    with tempfile.TemporaryDirectory(prefix='cod2-voice-reference-') as directory:
        temporary = Path(directory)
        # The exact official archive hash is checked before extraction.
        with tarfile.open(args.source_archive) as archive:
            archive.extractall(temporary)
        source = temporary / 'speex-1.0.4'
        flags = ['clang', '-O3', '-ffp-contract=off', '-fno-strict-aliasing',
                 '-I' + str(source / 'include'), '-I' + str(source / 'libspeex')]
        sources = sorted(p for p in (source / 'libspeex').glob('*.c') if not p.name.startswith('testenc'))
        library = temporary / 'upstream-1.0.4.dylib'
        subprocess.run(BACKGROUND + flags + ['-arch', 'arm64', '-dynamiclib'] +
                       list(map(str, sources)) + ['-o', str(library)], check=True, capture_output=True, text=True)
        probe_source = Path(__file__).with_name('codec_probe.c')
        probes = {}
        for arch in ['arm64', 'x86_64']:
            probes[arch] = temporary / ('probe-' + arch)
            subprocess.run(BACKGROUND + flags + ['-arch', arch, str(probe_source), '-o', str(probes[arch])],
                           check=True, capture_output=True, text=True)
        packets, decode, modern = [], [], []
        for mode in range(1, 9):
            metadata, frames = observe(probes['arm64'], library, 'encode', mode)
            assert metadata == dict(version='speex-1.0.4', bitstream_version=4)
            packets.append(dict(mode=mode, metadata=metadata, frames=frames))
            sequence = [frame['packet'] for frame in frames]
            for enhancement in ([0, 1] if mode in [2, 5, 8] else [0]):
                for loss in [False, True]:
                    incoming = sequence.copy()
                    if loss:
                        incoming[3:5] = ['-', '-']
                    _, expected = observe(probes['arm64'], library, 'decode', enhancement, incoming)
                    decode.append(dict(mode=mode, enhancement=enhancement, loss=loss,
                                       sequence=incoming, expected=expected))
            metadata, frames = observe(probes['x86_64'], args.modern_library.resolve(), 'encode', mode)
            assert metadata == dict(version='1.2.1', bitstream_version=4)
            modern.append(dict(mode=mode, metadata=metadata, frames=frames))
        # Synthetic mode-5 packet, with its five global-gain bits set to 31.
        gain_packet = bytearray.fromhex(packets[4]['frames'][0]['packet'])
        for bit in range(35, 40):
            gain_packet[bit // 8] |= 1 << (7 - bit % 8)
        fixture = dict(
            provenance=dict(encoder='Xiph Speex 1.0.4, unmodified official source',
                            url='https://downloads.xiph.org/releases/speex/speex-1.0.4.tar.gz',
                            sha256=SOURCE_SHA256,
                            compile='clang -arch arm64 -O3 -ffp-contract=off -fno-strict-aliasing',
                            input=INPUT, decoder_seed=17,
                            note='Synthetic source input only. No original-game output or native expected output.'),
            packets=packets, decode=decode, modern_packets=modern,
            modern_provenance=dict(encoder='Independent Speex 1.2.1, GStreamer runtime x86_64',
                                   library_sha256=MODERN_SHA256, input=INPUT,
                                   note='Bitstream version 4 acceptance vectors; this is not a PCM parity oracle for the original game.'),
            saturation_sequence=[gain_packet.hex()] * 3,
            framing_cases=[dict(packet='', result=-1), dict(packet='7f', result=-1),
                           dict(packet='48', result=-2), dict(packet='80', result=-1),
                           dict(packet='e0', result=-2),
                           dict(packet=packets[4]['frames'][0]['packet'][:2], result=0, remaining=-1)])
        args.output.write_text(json.dumps(fixture, separators=(',', ':')) + '\n')
    print(f'Synthetic voice fixture written to {args.output}')


if __name__ == '__main__':
    main()
