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


def tokenize_seeds():
    # Byte 0: low two bits pick tokenize / tokenize-with-limit / userinfo.
    return {
        'cmd-plain': b'\x00getstatus xxx',
        'cmd-quoted': b'\x00"quoted arg" // comment\n/* block */ tail',
        'cmd-connect': b'\x00connect "\\name\\player\\rate\\25000\\snaps\\20"',
        'cmd-limit': b'\x01rcon password a very long command line with many words',
        # Userinfo: op byte (bit0 picks big), key, value, then the seed info string.
        'info-small': b'\x02\x04name\x06player\\name\\x\\rate\\5000\\snaps\\20',
        'info-overwrite': b'\x02\x01a\x01b\\a\\1\\b\\2\\a\\3',
        'info-big': b'\x03\x08hostname\x04test' + b'\\key\\value' * 16,
    }


def _netchan_packet(payload, seq, sock_server, fragmented=False, start=0):
    body = struct.pack('<I', (seq | (1 << 31)) if fragmented else seq)
    if sock_server:
        body += struct.pack('<h', 0)          # qport, skipped on the server socket
    if fragmented:
        body += struct.pack('<h', start) + struct.pack('<h', len(payload))
    body += payload
    return struct.pack('<H', len(body)) + body


def netchan_seeds():
    tail = b'\x00\x00\x00\x00' * 3 + b'\x00'   # serverId/challenge/ack for SV_Netchan_Decode
    seeds = {
        'server-unfragmented': b'\x01' + _netchan_packet(b'reliable server payload', 1, True) + tail,
        'client-unfragmented': b'\x00' + _netchan_packet(b'\x01\x02\x03\x04server to client', 1, False),
    }
    # Two fragments: first is a full 0x514 (continue), the second is short (end).
    frag = b'\x01'
    frag += _netchan_packet(b'A' * 0x514, 5, True, fragmented=True, start=0)
    frag += _netchan_packet(b'B' * 16, 5, True, fragmented=True, start=0x514)
    frag += tail
    seeds['server-fragmented'] = frag
    return seeds


def oob_seeds():
    return {
        'getstatus': b'getstatus 1234',
        'getinfo': b'getinfo 1234',
        'getchallenge': b'getchallenge',
        'connect': b'connect "\\protocol\\118\\challenge\\1\\qport\\2\\name\\p"',
        'rcon-good': b'rcon secret status',
        'rcon-bad': b'rcon wrongpass map_rotate',
        'voice': b'v\x02\x01\x00\x10\x00' + b'voicebytes',
        'getstatus-long': b'getstatus ' + b'X' * 400,
    }


TARGETS = {
    'msg': msg_seeds,
    'tokenize': tokenize_seeds,
    'netchan': netchan_seeds,
    'oob': oob_seeds,
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
