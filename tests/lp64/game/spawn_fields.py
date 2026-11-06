#!/usr/bin/env python3
"""Check production entity spawn fields against Mac behavior with engine flags and sanitizers."""
from production_unit import run

run(__doc__, 'src/PC/game_mp/g_spawn_mp.c', 'tests/lp64/game/spawn_fields.c', ['brush', 'clientfields'])
