#!/usr/bin/env python3
"""Execute owned Mac 1.3 codec functions in a bounded i386 CPU emulator.

This is a codec oracle, not a Mach-O application runner. The game is never
started. All binary bytes stay in memory; observations belong outside Git.
Unicorn is an optional task-local dependency, not installed by this script.
"""
import collections
import ctypes
import hashlib
import math
from pathlib import Path
import struct
import subprocess

import unicorn
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP,
                               UC_X86_REG_FPCW, UC_X86_REG_MXCSR)

REFERENCE_SHA256 = '15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12'


class MacReference:
    def __init__(self, binary):
        self.binary = Path(binary)
        data = self.binary.read_bytes()
        if hashlib.sha256(data).hexdigest() != REFERENCE_SHA256:
            raise ValueError('This oracle supports only the audited Mac 1.3 binary')
        self.mu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        offset = 28
        for _ in range(struct.unpack_from('<I', data, 16)[0]):
            command, size = struct.unpack_from('<II', data, offset)
            if command == 1:
                segment = struct.unpack_from('<II16s8I', data, offset)
                address, vm_size, file_offset, file_size = segment[3:7]
                if address and vm_size:
                    start = address & ~4095
                    end = (address + vm_size + 4095) & ~4095
                    self.mu.mem_map(start, end - start)
                    self.mu.mem_write(address, data[file_offset:file_offset + file_size])
            offset += size
        self.mu.mem_map(0x40000000, 0x1000000)
        self.mu.mem_map(0x70000000, 0x10000)
        self.mu.mem_map(0x71000000, 0x1000)
        self.heap = 0x40000000
        self.stop = 0x71000000
        self.fp_return = self.stop + 16
        self.fp_capture = self.stop + 32
        self.scratch = self.stop + 128
        # Synthetic cdecl return thunks: import double -> x87; x87 -> double.
        self.mu.mem_write(self.fp_return, b'\xdd\x05' + struct.pack('<I', self.scratch) + b'\xc3')
        self.mu.mem_write(self.fp_capture, b'\xdd\x1d' + struct.pack('<I', self.scratch + 8) +
                          b'\xe9' + struct.pack('<i', self.stop - (self.fp_capture + 11)))
        self.mu.reg_write(UC_X86_REG_FPCW, 0x37f)
        self.mu.reg_write(UC_X86_REG_MXCSR, 0x1f80)
        self.symbols = {}
        for line in subprocess.check_output(['nm', str(self.binary)], text=True).splitlines():
            fields = line.split()
            if len(fields) == 3 and fields[0] != '00000000':
                self.symbols[fields[2]] = int(fields[0], 16)
        self.stubs = {}
        for line in subprocess.check_output(['otool', '-Iv', str(self.binary)], text=True).splitlines():
            fields = line.split()
            if len(fields) == 3 and fields[0].startswith('0x'):
                self.stubs[int(fields[0], 16)] = fields[2]
        self.models = {self.symbols['_Z_MallocInternal']: '_Z_MallocInternal',
                       self.symbols['_Z_FreeInternal']: '_Z_FreeInternal'}
        self.mu.hook_add(unicorn.UC_HOOK_CODE, self._import, begin=0x1acc000, end=0x1acd000)
        for address in self.models:
            self.mu.hook_add(unicorn.UC_HOOK_CODE, self._import, begin=address, end=address)
        self.imports = collections.Counter()
        self.libc = ctypes.CDLL(None)
        self.libc.srand.argtypes = [ctypes.c_uint]
        self.libc.rand.restype = ctypes.c_int
        self.libc.srand(17)

    def alloc(self, size):
        result = self.heap
        self.heap += (size + 15) & ~15
        if size < 0 or self.heap >= 0x41000000:
            raise RuntimeError('Oracle heap bound exceeded')
        self.mu.mem_write(result, bytes(size))
        return result

    def _u32(self, address):
        return struct.unpack('<I', self.mu.mem_read(address, 4))[0]

    def _import(self, mu, address, size, user):
        name = self.models.get(address, self.stubs.get(address))
        self.imports[name] += 1
        stack = mu.reg_read(UC_X86_REG_ESP)

        def args(count):
            return struct.unpack('<' + 'I' * count, mu.mem_read(stack + 4, count * 4))

        result = 0
        if name == '_Z_MallocInternal':
            # Its audited body calls malloc followed by Com_Memset(..., 0, n).
            result = self.alloc(args(1)[0])
        elif name == '_Z_FreeInternal':
            pass  # Bounded arena survives this short, isolated oracle process.
        elif name in ('_memmove', '_memcpy'):
            destination, source, length = args(3)
            mu.mem_write(destination, bytes(mu.mem_read(source, length)))
            result = destination
        elif name == '_rand':
            result = self.libc.rand()
        elif name == '_fprintf':
            pass  # Warning text has no codec state effect; calls are counted.
        elif name in ('_exp', '_cos', '_acos', '_sin', '_sqrt', '_log', '_floor'):
            value = struct.unpack('<d', mu.mem_read(stack + 4, 8))[0]
            mu.mem_write(self.scratch, struct.pack('<d', getattr(math, name[1:])(value)))
            mu.reg_write(UC_X86_REG_EIP, self.fp_return)
            return
        else:
            raise RuntimeError(f'Unmodeled import {name!r} at {address:#x}')
        mu.reg_write(UC_X86_REG_EAX, result)
        mu.reg_write(UC_X86_REG_EIP, self._u32(stack))
        mu.reg_write(UC_X86_REG_ESP, stack + 4)

    def call(self, function, *args, floating=False):
        address = self.symbols.get(function, function)
        stack = 0x7000effc
        target = self.fp_capture if floating else self.stop
        self.mu.mem_write(stack, struct.pack('<' + 'I' * (len(args) + 1), target, *args))
        self.mu.reg_write(UC_X86_REG_ESP, stack)
        try:
            self.mu.emu_start(address, self.stop, timeout=10000000, count=3000000)
        except unicorn.UcError as error:
            raise RuntimeError(f'{function}: {error} at {self.mu.reg_read(UC_X86_REG_EIP):#x}') from error
        if self.mu.reg_read(UC_X86_REG_EIP) != self.stop:
            raise RuntimeError('Oracle instruction/time bound exceeded')
        if floating:
            return struct.unpack('<d', self.mu.mem_read(self.scratch + 8, 8))[0]
        result = self.mu.reg_read(UC_X86_REG_EAX)
        return result - 0x100000000 if result & 0x80000000 else result

    def self_test(self):
        data, output = self.alloc(16), self.alloc(16)
        self.mu.mem_write(data, struct.pack('<4f', 3, 4, 0, 0))
        rms = self.call('__Z11compute_rmsPKfi', data, 4, floating=True)
        # Mac compute_rms adds its 0.1 floor after the mean square.
        expected_rms = struct.unpack('<f', struct.pack('<f', math.sqrt(6.25 + 0.1)))[0]
        assert rms == expected_rms, rms
        self.call('__Z10signal_mulPKfPffi', data, output, 0x40000000, 4)
        assert struct.unpack('<4f', self.mu.mem_read(output, 16)) == (6, 8, 0, 0)
        second = self.alloc(16)
        self.mu.mem_write(second, struct.pack('<4f', 7, 8, 4, 4))
        self.call('__Z15lsp_interpolatePfS_S_iii', data, second, output, 4, 1, 4)
        assert struct.unpack('<4f', self.mu.mem_read(output, 16)) == (5, 6, 2, 2)
        exp_address = next(address for address, name in self.stubs.items() if name == '_exp')
        exp_result = self.call(exp_address, *struct.unpack('<2I', struct.pack('<d', 1)), floating=True)
        assert exp_result == math.exp(1)
        # Separately check overlapping memmove and the deterministic rand model.
        self.mu.mem_write(data, b'0123456789abcdef')
        memmove = next(address for address, name in self.stubs.items() if name == '_memmove')
        self.call(memmove, data + 2, data, 8)
        assert bytes(self.mu.mem_read(data, 10)) == b'0101234567'
        self.libc.srand(17)
        self.imports.clear()

    def decode(self, packets, enhancement):
        state = self.call('__Z18speex_decoder_initPK9SpeexMode', self.symbols['_speex_nb_mode'])
        option = self.alloc(4)
        self.mu.mem_write(option, struct.pack('<I', enhancement))
        assert self.call('__Z17speex_decoder_ctlPviS_', state, 0, option) == 0
        bits, output = self.alloc(36), self.alloc(640)
        self.call('__Z15speex_bits_initP9SpeexBits', bits)
        self.libc.srand(17)
        frames = []
        for index, packet in enumerate(packets):
            lost = packet == '-'
            if not lost:
                raw = bytes.fromhex(packet)
                buffer = self.alloc(max(1, len(raw)))
                self.mu.mem_write(buffer, raw)
                self.call('__Z20speex_bits_read_fromP9SpeexBitsPci', bits, buffer, len(raw))
            self.mu.mem_write(output, bytes(640))
            result = self.call('__Z12speex_decodePvP9SpeexBitsPf', state, 0 if lost else bits, output)
            remaining = 0 if lost else self.call('__Z20speex_bits_remainingP9SpeexBits', bits)
            pcm = list(struct.unpack('<160f', self.mu.mem_read(output, 640)))
            if not all(math.isfinite(sample) for sample in pcm):
                raise RuntimeError('Nonfinite reference output')
            frames.append(dict(frame=index, lost=lost, result=result, remaining=remaining, pcm=pcm))
        self.call('__Z18speex_bits_destroyP9SpeexBits', bits)
        self.call('__Z21speex_decoder_destroyPv', state)
        return frames

    def metadata(self):
        return dict(binary_sha256=REFERENCE_SHA256, unicorn=unicorn.__version__,
                    imports=dict(self.imports), self_test='passed',
                    model='i386 SSE/x87; zeroing arena; host libm and libc rand seeded 17; warnings counted')
