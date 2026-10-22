#!/usr/bin/env python3
"""Checks the world can be played: every room both ways, every gate shut without its ability and
open with it, every gem reached. Uses the level bot (tools/levelbot.cpp, built as
build/Release/levelbot), several at once. Takes a few minutes.

  python3 tools/check_world.py                # all of it
  python3 tools/check_world.py crevasse_camp  # only the routes through rooms whose name has that

Each route below is: the room, where to start ('S' for its Start, or "column row" counted from the
top-left), the goal (L, R, T or B for a way out, "pickup", or "gem N"), the abilities the player has,
and whether the bot should get there. "<room> --open" has the room's gates open. When you change a
room, add or fix its routes here.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

# What you have by each part of the world: the rope from the crevasse, the long rope from the end of
# the garden (east), the chalk bag from the Archive (west), the throwing hand at the Summit, the
# feather in Backtrack.
R = 'rope'
RL = 'rope,long_rope'
RLC = 'rope,long_rope,wall_climb'
RLCA = 'rope,long_rope,wall_climb,air_throw'
ALL = 'rope,long_rope,wall_climb,air_throw,double_jump'

ROUTES = [
    # The Crevasse, where a new game starts: walking, jumping, Ash's rope and the first swings.
    ('crevasse_wake', 'S', 'R', 'none', True),
    ('crevasse_steps', 'S', 'R', 'none', True),
    ('crevasse_steps', '37 13', 'L', 'none', True),
    ('crevasse_gaps', 'S', 'R', 'none', True),
    ('crevasse_gaps', '41 16', 'L', 'none', True),
    ('crevasse_camp', 'S', 'pickup', 'none', True),
    ('crevasse_camp', 'S', 'R', 'none', False),   # the pit needs the rope
    ('crevasse_camp', 'S', 'R', R, True),
    ('crevasse_camp', '45 17', 'L', R, True),
    ('crevasse_dips', 'S', 'R', 'none', True),    # practice: walkable, swingable
    ('crevasse_dips', 'S', 'gem 0', R, True),
    ('crevasse_dips', '41 16', 'L', R, True),
    ('crevasse_chasm', 'S', 'R', 'none', False),
    ('crevasse_chasm', 'S', 'R', R, True),
    ('crevasse_chasm', '48 18', 'L', R, True),
    # The Cave and The Well, up to the Crossroads: the rope, thrown standing.
    ('level1', 'S', 'R', R, True),
    ('level1', '46 14', 'L', R, True),
    ('level1', 'S', 'gem 0', R, True),
    ('level2', 'S', 'T', R, True),
    ('level2', '4 3', 'L', R, True),
    ('level2', 'S', 'gem 0', R, True),
    # The Crossroads.
    ('crossroads', 'S', 'L', R, True),
    ('crossroads', 'S', 'R', R, True),
    ('crossroads', 'S', 'T', R, True),
    ('crossroads', 'S', 'B', R, True),
    ('crossroads', '2 26', 'R', R, True),
    ('crossroads', '57 26', 'L', R, True),
    ('crossroads', 'S', 'gem 0', R, True),
    # The ruins, west, and the chalk bag at the top of the Archive.
    ('ruins_gate', 'S', 'L', R, True),
    ('ruins_gate', '2 18', 'R', R, True),
    ('ruins_gate', 'S', 'gem 0', R, True),
    ('ruins_hollow', 'S', 'L', R, True),
    ('ruins_hollow', '3 8', 'R', R, True),
    ('ruins_hollow', 'S', 'gem 0', R, True),
    ('ruins_archive', 'S', 'pickup', R, True),
    ('ruins_archive', 'S', 'R', R, True),
    ('ruins_archive', 'S', 'gem 0', R, True),
    # The garden, east, and the long rope down in Curtains' hollow: out of it only with it.
    ('level3', 'S', 'R', R, True),
    ('level3', '57 18', 'L', R, True),
    ('level3', 'S', 'gem 0', R, True),
    ('level4', 'S', 'pickup', R, True),
    ('level4', '50 22', 'L', R, False),
    ('level4', '50 22', 'L', RL, True),
    ('level4', 'S', 'gem 0', R, True),
    # The Tall Hall: shut without the long rope, even climbing.
    ('tall_hall', 'S', 'T', 'rope,wall_climb', False),
    ('tall_hall', 'S', 'T', RL, True),
    ('tall_hall', 'S', 'gem 0', RL, True),
    ('the_drop', 'S', 'T', ALL, False),           # down only
    # The Summit: the throwing hand up a chimney that needs climbing; the way on needs it.
    ('level5', '4 28', 'pickup', RL, False),
    ('level5', '4 28', 'pickup', RLC, True),
    ('level5', '4 28', 'R', RLC, False),
    ('level5', '4 28', 'R', RLCA, True),
    ('level5', '4 28', 'gem 0', RLC, True),
    # The mountains, the ember chimney and the pendulums.
    ('level6', 'S', 'R', RLCA, True),
    ('level6', '57 22', 'L', RLCA, True),
    ('level5', '61 17', 'B', RLCA, True),
    ('level6', 'S', 'gem 0', RLCA, True),
    ('level7', 'S', 'T', RLCA, True),
    ('level7', 'S', 'gem 0', RLCA, True),
    ('level8', 'S', 'R', RLCA, True),
    ('level8', '57 20', 'B', RLCA, True),
    ('level8', 'S', 'gem 0', RLCA, True),
    # Backtrack and the feather; the Windy Ledge, shut without it; Daylight and the way out.
    ('level9', 'S', 'pickup', RLCA, True),
    ('level9', 'S', 'R', RLCA, True),
    ('level9', '49 14', 'L', ALL, True),
    ('level9', 'S', 'gem 0', RLCA, True),
    ('windy_ledge', 'S', 'R', RLCA, False),
    ('windy_ledge', 'S', 'R', ALL, True),
    ('windy_ledge', '52 14', 'L', ALL, True),
    ('windy_ledge', 'S', 'gem 0', ALL, True),
    ('level10', 'S', 'T', ALL, True),
]


def run(route, bot):
    room, start, goal, abilities, expected = route
    args = [bot] + room.split() + ['--abilities', abilities, '--goal'] + goal.split()
    if start != 'S':
        args += ['--start'] + start.split()
    out = subprocess.run(args, capture_output=True, text=True)
    reached = out.returncode == 0
    if out.returncode not in (0, 1):
        return route, None, out.stderr.strip()
    return route, reached, out.stdout


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
    bot = os.path.join(root, 'build', 'Release', 'levelbot')
    if not os.path.exists(bot):
        sys.exit('Build first: cmake --build --preset conan-release (makes build/Release/levelbot)')
    only = sys.argv[1] if len(sys.argv) > 1 else ''
    routes = [r for r in ROUTES if only in r[0]]
    failed = 0
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        for route, reached, output in pool.map(lambda r: run(r, bot), routes):
            room, start, goal, abilities, expected = route
            ok = reached == expected
            failed += not ok
            what = 'reached' if reached else 'not reached' if reached is not None else 'error'
            print(f"{'ok  ' if ok else 'FAIL'} {room:<14} from {start:<7} to {goal:<7} with {abilities:<45} {what}", flush=True)
            if not ok:
                print('\n'.join('     ' + line for line in output.splitlines()[:60]))
    print(f'{len(routes) - failed} of {len(routes)} as expected')
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
