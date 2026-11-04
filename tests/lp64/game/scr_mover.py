#!/usr/bin/env python3
"""Check production script mover commands against Mac behavior with engine flags and sanitizers."""
from production_unit import run

run(__doc__, 'src/PC/game_mp/g_scr_mover_mp.c', 'tests/lp64/game/scr_mover.c',
    ['gravity', 'accel', 'moving', 'plain'])
