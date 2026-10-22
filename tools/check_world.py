#!/usr/bin/env python3
"""Checks the world can be played: every room both ways, every gate shut without its ability and
open with it, every gem reached. Uses the level bot (tools/levelbot.cpp, built as
build/Release/levelbot), several at once. Takes a few minutes.

  python3 tools/check_world.py            # all of it
  python3 tools/check_world.py level3     # only the routes through rooms whose name has "level3"

Each route below is: the room, where to start ('S' for its Start, or "column row" counted from the
top-left), the goal (L, R, T or B for a way out, "pickup", or "gem N"), the abilities the player has,
and whether the bot should get there. "<room> --open" has the room's gates open. When you change a
room, add or fix its routes here.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ALL = 'climbing_pick,long_rope,double_jump'
PICK = 'climbing_pick'
PICK_ROPE = 'climbing_pick,long_rope'

ROUTES = [
    # The Cave: the pick can be reached without it, the way on can't.
    ('level1', 'S', 'pickup', 'none', True),
    ('level1', 'S', 'R', 'none', False),
    ('level1', '74 49', 'R', PICK, True),
    ('level1', 'S', 'gem 0', PICK, True),
    ('level1', 'S', 'gem 1', 'none', False),
    ('level1', 'S', 'gem 1', PICK, True),
    ('level2', 'S', 'T', PICK, True),
    ('level2', '10 1', 'L', PICK, True),
    # The Crossroads: every way out, from where you come in.
    ('crossroads', 'S', 'R', PICK, True),
    ('crossroads', 'S', 'L', PICK, True),
    ('crossroads', 'S', 'T', PICK, True),
    ('crossroads', '10 21', 'R', PICK, True),
    ('crossroads', '46 21', 'B', PICK, True),
    ('crossroads', 'S', 'gem 0', PICK, True),
    # The ruins, west: the pick is all they need.
    ('ruins_gate', '60 21', 'L', PICK, True),
    ('ruins_gate', '2 22', 'R', PICK, True),
    ('ruins_gate', '60 21', 'gem 0', PICK, True),
    ('ruins_hollow', '44 9', 'L', PICK, True),
    ('ruins_hollow', '2 39', 'R', PICK, True),
    ('ruins_hollow', '44 9', 'gem 0', PICK, True),
    ('ruins_archive', 'S', 'pickup', PICK, True),
    ('ruins_archive', 'S', 'R', PICK, True),
    ('ruins_archive', 'S', 'gem 0', PICK, True),
    # The garden, east, and the long rope at its end.
    ('level3', 'S', 'R', PICK, True),
    ('level3', '150 27', 'L', PICK, True),
    ('level4', 'S', 'pickup', PICK, True),
    ('level4', '142 25', 'L', PICK_ROPE, True),
    # The Tall Hall: shut without the long rope.
    ('tall_hall', '7 24', 'R', PICK, False),
    ('tall_hall', '7 24', 'R', PICK_ROPE, True),
    ('tall_hall', '13 5', 'B', PICK_ROPE, True),
    ('tall_hall', '7 24', 'gem 0', PICK_ROPE, True),
    # The Drop: down only.
    ('the_drop', '3 11', 'T', ALL, False),
    ('level5 --open', '7 61', 'B', PICK_ROPE, True),
    # The mountains.
    ('level5', '7 61', 'R', PICK_ROPE, True),
    ('level5', '130 21', 'L', PICK_ROPE, True),
    ('level6', 'S', 'R', PICK_ROPE, True),
    ('level6', '78 6', 'L', PICK_ROPE, True),
    ('level7', 'S', 'T', PICK_ROPE, True),
    ('level7', '15 1', 'L', PICK_ROPE, True),
    ('level8', '1 24', 'R', PICK_ROPE, True),
    ('level8', '108 13', 'B', PICK_ROPE, True),
    ('level9', 'S', 'R', PICK_ROPE, True),
    ('level9', '88 11', 'L', PICK_ROPE, True),
    # The Windy Ledge: shut without the double jump.
    ('windy_ledge', 'S', 'R', PICK_ROPE, False),
    ('windy_ledge', 'S', 'R', ALL, True),
    ('windy_ledge', '52 26', 'L', ALL, True),
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
