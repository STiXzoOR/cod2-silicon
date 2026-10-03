"""Separate true LP64 scalar values from spurious ILP32 blob relocations."""
from dataclasses import replace
import json

from layout import pointer_offsets, shape
from stabs import Unsupported

SCALAR_RELOCATIONS = {
    'sGerman_ISO_VK_Map', 'sFrench_ISO_VK_Map', 'sANSI_VK_Map',
    'FastTranslateTbl', 'sD3DTextureOpToOpenGL', 'infoParms',
    's_XModelSurfaceSize', 'virtualKeyConvert', 'dlText',
    'g_encoder_samplerate', 'g_sound_recordFrequency', 'inflate_mask', 'fixed_td',
}


def recover_object(obj, db, byname, reference):
    choices = {}
    for variable in byname.get(obj.name, []):
        try:
            ty = shape(db, variable.type)
            choices[json.dumps(ty, sort_keys=True)] = ty
        except (Unsupported, RecursionError):
            pass
    if len(choices) != 1:
        raise ValueError(obj.name + ': scalar recovery requires a unique debug layout')
    ty = next(iter(choices.values()))
    # The owning engine TU defines 54 infoParm_t entries (including two zero
    # sentinels). Old STABS describes only 53; native output already omits it.
    if obj.name == 'infoParms':
        if ty['kind'] != 'array' or ty['count'] != 53 or ty['child']['size'] != 20:
            raise ValueError('infoParms: unexpected debug table layout')
        ty = dict(ty, count=54, size=54 * ty['child']['size'])
    if ty['size'] > len(obj.data):
        raise ValueError(obj.name + ': debug layout exceeds source object')
    truth, record = reference.read_object(obj.name, ty['size'])
    # Check the entire source span wherever the reference has object storage;
    # source-only tail padding is allowed only when zero and unrelocated.
    checked_size = min(len(obj.data), record['next_symbol_extent'])
    checked, _ = reference.read_object(obj.name, checked_size)
    relocated = {i for offset in obj.relocs for i in range(offset, offset + 4)}
    mismatches = [(i, byte, checked[i] if i < checked_size else None)
                  for i, byte in enumerate(obj.data)
                  if byte and i not in relocated and (i >= checked_size or byte != checked[i])]
    if mismatches:
        raise ValueError('%s: nonzero non-relocated bytes disagree with reference: %s' %
                         (obj.name, mismatches))
    pointers = pointer_offsets(ty)
    pointer_bytes = {i for offset in pointers for i in range(offset, offset + 4)}
    false = set(obj.relocs) - pointers
    if not false or any(offset + 4 > ty['size'] for offset in false):
        raise ValueError(obj.name + ': unexpected scalar relocation offsets')
    if any(i in pointer_bytes for offset in false for i in range(offset, offset + 4)):
        raise ValueError(obj.name + ': false relocation overlaps a pointer field')
    data = bytearray(obj.data)
    for index, byte in enumerate(truth):
        if index not in pointer_bytes:
            data[index] = byte
    relocs = {offset: value for offset, value in obj.relocs.items() if offset in pointers}
    recovered = replace(obj, data=bytes(data), relocs=relocs)
    record.update(name=obj.name, false_relocations=sorted(false),
                  genuine_relocations=sorted(relocs), nonzero_byte_mismatches=[],
                  checked_bytes=checked_size, source_padding_bytes=len(obj.data) - checked_size)
    return recovered, ty, record
