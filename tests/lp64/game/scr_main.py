#!/usr/bin/env python3
"""Check production script builtins against Mac behavior with engine flags and sanitizers."""
from production_unit import run

run(__doc__, 'src/PC/game_mp/g_scr_main_mp.c', 'tests/lp64/game/scr_main.c',
    ['table', 'trig', 'shader', 'spawn', 'playfx', 'loopsound', 'radius'])
