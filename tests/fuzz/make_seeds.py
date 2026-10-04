#!/usr/bin/env python3
"""Write the synthetic seed corpora for the fuzz targets.

Every seed is built here from the protocol's documented framing; none comes
from a capture of a real server or player. Usage: make_seeds.py OUTPUT_DIR
(writes OUTPUT_DIR/<target>/<name>).
"""
from pathlib import Path
import struct
import sys


class BitWriter:
    """Mirrors msg_mp.c: bit groups open a fresh byte, byte writes append."""

    def __init__(self):
        self.data = bytearray()
        self.bit = 0

    def bits(self, value, count):
        for _ in range(count):
            if self.bit & 7 == 0:
                self.bit = len(self.data) * 8
                self.data.append(0)
            if value & 1:
                self.data[self.bit >> 3] |= 1 << (self.bit & 7)
            self.bit += 1
            value >>= 1
        return self

    def byte(self, value):
        self.data.append(value & 0xff)
        return self

    def short(self, value):
        self.data += struct.pack('<h', value)
        return self

    def long(self, value):
        self.data += struct.pack('<i', value)
        return self

    def raw(self, data):
        self.data += data
        return self

    def string(self, text):
        self.data += text.encode('latin-1') + b'\0'
        return self


def msg_seeds():
    seeds = {
        'decode-text': b'\x00' + b'userinfo "\\name\\player\\rate\\25000\\snaps\\20"',
        'decode-bytes': b'\x00' + bytes(range(256)),
        'decode-pattern': b'\x00' + b'\x55\xaa' * 64,
        'read-strings': b'\x01' + BitWriter().byte(3).string('print "hello"')
                        .byte(4).string('\\sv_hostname\\test\\g_gametype\\tdm')
                        .byte(5).raw(b'getstatus 123\n').data,
    }
    # Entity delta: number, not removed, changed, three fields from the table.
    entity = BitWriter().byte(6).bits(37, 10).bits(0, 1).bits(1, 1).byte(3)
    entity.bits(1, 1).bits(1, 1).long(12345)          # pos.trTime: changed, 32-bit value
    entity.bits(1, 1).bits(1, 1).bits(0, 1).bits(20, 5).byte(140)
    entity.bits(0, 1)
    seeds['read-entity'] = b'\x01' + bytes(entity.data)
    client = BitWriter().byte(7).bits(5, 6).bits(0, 1).bits(1, 1).byte(2).bits(1, 1).bits(1, 1).bits(3, 2)
    seeds['read-client'] = b'\x01' + bytes(client.data)
    archived = BitWriter().byte(8).bits(500, 10).bits(0, 1).bits(1, 1).byte(1).bits(1, 1).bits(1, 1).long(7)
    seeds['read-archived'] = b'\x01' + bytes(archived.data)
    # Playerstate: two changed fields, stats, ammo, no objectives or HUD.
    ps = BitWriter().byte(9).byte(2).bits(1, 1).long(1000).bits(1, 1).bits(1, 1).long(0x41200000)
    ps.bits(1, 1).bits(0x21, 6).short(100).short(5)
    ps.bits(1, 1).bits(1, 1).short(3).short(30).short(90)
    ps.bits(0, 4).bits(0, 1).bits(0, 1)
    seeds['read-playerstate'] = b'\x01' + bytes(ps.data)
    hud = BitWriter().byte(26).byte(0).bits(0, 1).bits(0, 1).bits(0, 4).bits(0, 1).bits(1, 1)
    hud.bits(1, 5).bits(3, 5).bits(1, 1).bits(1, 1).raw(b'\x10\x00\x00\x00').bits(0, 4).bits(0, 5)
    seeds['read-hud'] = b'\x01' + bytes(hud.data)
    cmd = BitWriter().byte(11).long(0x1234).bits(0, 1).long(5000).bits(1, 1).bits(0, 1).bits(1, 1)
    cmd.bits(1, 1).bits(1, 1).short(100).bits(0, 1).bits(1, 1).bits(5, 4)
    seeds['read-usercmd'] = b'\x01' + bytes(cmd.data)
    return seeds


TARGETS = {
    'msg': msg_seeds,
}


def main():
    out = Path(sys.argv[1])
    for target, make in TARGETS.items():
        directory = out / target
        directory.mkdir(parents=True, exist_ok=True)
        for name, data in make().items():
            (directory / name).write_bytes(bytes(data))


if __name__ == '__main__':
    main()
