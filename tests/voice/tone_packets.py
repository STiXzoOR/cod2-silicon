#!/usr/bin/env python3
"""Regenerate the synthetic playback input with unmodified Xiph Speex 1.0.4."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

from regenerate import SOURCE_SHA256
from run import BACKGROUND

PROBE = r'''
#include <speex/speex.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /* Integer period-16 sine: 512 Hz at 8192 Hz, no host libm dependency. */
    const short period[16] = {0, 3061, 5657, 7391, 8000, 7391, 5657, 3061,
                             0, -3061, -5657, -7391, -8000, -7391, -5657, -3061};
    void *encoder = speex_encoder_init(&speex_nb_mode);
    int mode = 5, rate = 8192, count = 0;
    assert(encoder && !speex_encoder_ctl(encoder, SPEEX_SET_MODE, &mode));
    assert(!speex_encoder_ctl(encoder, SPEEX_SET_SAMPLING_RATE, &rate));
    assert(!speex_encoder_ctl(encoder, SPEEX_GET_FRAME_SIZE, &count) && count == 160);
    SpeexBits bits;
    speex_bits_init(&bits);
    srand(17);
    for (int frame = 0; frame < 64; frame++) {
        float pcm[160];
        char packet[256];
        for (int i = 0; i < 160; i++)
            pcm[i] = period[i % 16];
        speex_bits_reset(&bits);
        assert(speex_encode(encoder, pcm, &bits) == 1);
        int length = speex_bits_write(&bits, packet, sizeof(packet));
        assert(length > 0 && length < 256);
        for (int i = 0; i < length; i++)
            printf("%02x", (unsigned char)packet[i]);
        putchar('\n');
    }
    speex_bits_destroy(&bits);
    speex_encoder_destroy(encoder);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-archive', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if hashlib.sha256(args.source_archive.read_bytes()).hexdigest() != SOURCE_SHA256:
        parser.error('Unrecognized Xiph source archive')
    with tempfile.TemporaryDirectory(prefix='cod2-voice-tone-') as temporary:
        build = Path(temporary)
        with tarfile.open(args.source_archive) as archive:
            archive.extractall(build)
        source = build / 'speex-1.0.4'
        probe = build / 'tone.c'
        probe.write_text(PROBE)
        sources = sorted(p for p in (source / 'libspeex').glob('*.c') if not p.name.startswith('testenc'))
        exe = build / 'tone'
        command = ['clang', '-arch', 'arm64', '-O3', '-ffp-contract=off', '-fno-strict-aliasing',
                   '-I' + str(source / 'include'), '-I' + str(source / 'libspeex'),
                   str(probe), *map(str, sources), '-o', str(exe)]
        subprocess.run(BACKGROUND + command, check=True, capture_output=True, text=True)
        packets = subprocess.check_output(BACKGROUND + [str(exe)], text=True).splitlines()
    assert len(packets) == 64
    fixture = dict(provenance=dict(
        encoder='Unmodified Xiph Speex 1.0.4, narrowband mode 5; srand(17)',
        url='https://downloads.xiph.org/releases/speex/speex-1.0.4.tar.gz',
        sha256=SOURCE_SHA256,
        compile='clang -arch arm64 -O3 -ffp-contract=off -fno-strict-aliasing',
        input='tone_packets.py PROBE integer period-16 sine, amplitude 8000',
        note='Synthetic packets only; no decoded PCM or game-derived input'),
        sample_rate=8192, tone_hz=512, frame_samples=160, packets=packets)
    args.output.write_text(json.dumps(fixture, indent=2) + '\n')


if __name__ == '__main__':
    main()
