#!/usr/bin/env python3
"""Check production server console commands against Mac behavior with engine flags and sanitizers."""
from production_unit import run

run(__doc__, 'src/PC/game_mp/g_svcmds_mp.c', 'tests/lp64/game/svcmds.c',
    ['listip', 'say', 'usage', 'entitylist'])
