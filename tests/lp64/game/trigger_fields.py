#!/usr/bin/env python3
"""Check production trigger setup against Mac behavior with engine flags and sanitizers."""
from production_unit import run

run(__doc__, 'src/PC/game_mp/g_trigger_mp.c', 'tests/lp64/game/trigger_fields.c', ['lookat', 'wait'])
