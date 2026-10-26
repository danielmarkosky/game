#!/usr/bin/env python3
"""Checks the world can be played: every room both ways, every gate shut without its ability and
open with it, every gem reached. Uses the level bot (tools/levelbot.cpp, built as
build/Release/levelbot), several at once. Takes a few minutes.

  python3 tools/check_world.py                # all of it
  python3 tools/check_world.py old_road               # only the routes through rooms whose name has that

Each route below is: the room, where to start ('S' for its Start, or "column row" counted from the
top-left), the goal (L, R, T or B for a way out, "L:21" for the left door starting 21 up, "pickup",
"gem N", "at COL ROW" for standing in that tile, "cart" or "stop"), the abilities the player has,
whether the bot should get there, and maybe more for the bot (RIDING: in a cart). "<room> --open"
has the room's gates open. When you change a room, add or fix its routes here.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

# What you have by each part of the world: nothing at first, then the rope Odo gives you on the
# Old Road. Then from the two routes open from the Campfire, the long rope (the Hook Garden) and the
# chalk bag (the Hollows), in either order; the long rope opens the Frost Caves (the glove at their
# end), the chalk bag the Sky Road (the feather at its end); all four, the Old Line. "diver" is the
# diving suit, from Mags's stall.
R = 'rope'
RL = 'rope,long_rope'
RC = 'rope,wall_climb'
RLC = 'rope,long_rope,wall_climb'
RLA = 'rope,long_rope,air_throw'
RLCA = 'rope,long_rope,wall_climb,air_throw'
ALL = 'rope,long_rope,wall_climb,air_throw,double_jump'
RIDING = ['--riding', '1']  # in a mine cart rolling right

ROUTES = [
    # The Hidden Lake, where a new game starts: you come to in the water, swim ashore and walk out.
    ('lake', 'S', 'R', 'none', True),
    # Its secrets, behind the waterfall: the wall that isn't there, to the Still, with nothing at all;
    # up inside it to the passage to the Hidden Spring, with the long rope and not before; and back.
    ('lake', 'S', 'L:7', 'none', True),
    ('lake', 'S', 'L:21', R, False),
    ('lake', 'S', 'L:21', RL, True),
    ('lake', '2 4', 'R', RL, True),
    # A gem up under the overhang by the way out: for a rope that reaches.
    ('lake', 'S', 'gem 0', R, False),
    ('lake', 'S', 'gem 0', RL, True),
    # The Drowned Grotto, down the crack in the lake bed: too far under water for a breath, not for
    # the diving suit's; there and back.
    ('drowned', '3 1', 'gem 0', R, False),
    ('drowned', '3 1', 'gem 0', R + ',diver', True),
    ('drowned', '42 14', 'T', R + ',diver', True),
    # The Still: floating, nothing needed, all the way to the stopwatch and back.
    ('still', 'S', 'L', 'none', True),
    ('still', 'S', 'gem 0', 'none', True),
    ('still', 'S', 'gem 1', 'none', True),
    ('still', 'S', 'gem 2', 'none', True),
    ('falls_source', 'S', 'pickup', 'none', True),
    ('falls_source', 'S', 'gem 0', 'none', True),
    ('falls_source', 'S', 'gem 1', 'none', True),
    ('falls_source', '3 21', 'R', 'none', True),
    ('still', '1 20', 'R', 'none', True),
    # The Hidden Spring: its gems (numbered from the top: the one over the pool, the stack's, for
    # when you have the feather, the island's).
    ('hidden_spring', 'S', 'gem 0', R, False),
    ('hidden_spring', 'S', 'gem 0', RL, True),
    ('hidden_spring', 'S', 'gem 1', RL, False),
    ('hidden_spring', 'S', 'gem 1', ALL, True),
    ('hidden_spring', 'S', 'gem 2', RL, True),
    # The Old Road: Odo gives you the rope, and the ditch where the bridge was needs it, both ways.
    ('old_road', 'S', 'R', 'none', False),
    ('old_road', 'S', 'R', R, True),
    ('old_road', '53 22', 'L', R, True),
    ('old_road', 'S', 'gem 0', R, True),
    # The Gully: a swing over a ditch, reeling in up onto a ledge, a long swing over the pool.
    ('gully', 'S', 'R', 'none', False),
    ('gully', 'S', 'R', R, True),
    ('gully', '61 11', 'L', R, True),
    ('gully', 'S', 'gem 0', R, True),
    # The Campfire: the ways on, all open to anyone (it's what they need that's in their rooms).
    ('camp', 'S', 'R:8', 'none', True),
    ('camp', 'S', 'B', 'none', True),
    ('camp', 'S', 'T', 'none', True),
    ('camp', 'S', 'R:32', 'none', True),
    ('camp', '45 31', 'L:31', 'none', True),
    ('camp', 'S', 'gem 0', 'none', True),
    ('camp', 'S', 'gem 1', 'none', True),
    # The Hook Garden: two swings, mushrooms, rings, to the long rope. The ledge in the air: the long
    # rope. The hollow in the glade's roof: the feather, or the long rope.
    ('hook_garden', 'S', 'R', 'none', False),
    ('hook_garden', 'S', 'R', R, True),
    ('hook_garden', 'S', 'gem 0', R, False),
    ('hook_garden', 'S', 'gem 0', RL, True),
    ('mushroom_glade', 'S', 'R', R, True),
    ('mushroom_glade', 'S', 'gem 0', R, False),
    ('mushroom_glade', 'S', 'gem 1', R, True),
    ('ring_orchard', 'S', 'pickup', R, True),
    ('ring_orchard', 'S', 'gem 0', R, True),
    ('ring_orchard', 'S', 'gem 1', R, True),
    # The Hollows: over the spikes to the dark, over crumbling stone to the den; the chalk bag; up
    # to Pip with it. Back up out of the first room: climbing. The dark's ledge: the long rope.
    ('hollows', 'S', 'L', R, True),
    ('hollows', '2 25', 'T', R, False),
    ('hollows', '2 25', 'T', RC, True),
    ('dark_hollow', 'S', 'L', R, True),
    ('dark_hollow', 'S', 'gem 0', R, False),
    ('dark_hollow', 'S', 'gem 0', RL, True),
    ('pips_den', 'S', 'pickup', R, True),
    ('pips_den', 'S', 'at 20 14', R, False),
    ('pips_den', 'S', 'at 20 14', RC, True),
    ('pips_den', 'S', 'gem 1', R, True),
    # The Frost Caves: the long rope across the chasm, and not without it. Ice, icicles, the
    # glove. Gems on top of slick stone: the feather's.
    ('frost_caves', 'S', 'L', RC, False),
    ('frost_caves', 'S', 'L', RL, True),
    ('frost_caves', 'S', 'gem 0', RLCA, False),
    ('frost_caves', 'S', 'gem 0', ALL, True),
    ('icicle_hall', 'S', 'L', RL, True),
    ('icicle_hall', 'S', 'gem 0', RL, True),
    ('frozen_falls', 'S', 'pickup', RL, True),
    ('frozen_falls', 'S', 'gem 0', RLCA, False),
    ('frozen_falls', 'S', 'gem 0', ALL, True),
    # The Sky Road: the climb, and nothing else gets you up it; clouds, updrafts, the feather. The
    # gem over the peak: a throw from a jump (the glove).
    ('sky_road', 'S', 'R', RL, False),
    ('sky_road', 'S', 'R', RC, True),
    ('sky_road', 'S', 'gem 0', RC, True),
    ('updraft_cliffs', 'S', 'T', RC, True),
    ('updraft_cliffs', 'S', 'gem 0', RC, True),
    ('sky_edge', 'S', 'pickup', RC, True),
    ('sky_edge', 'S', 'gem 0', RLC, False),
    ('sky_edge', 'S', 'gem 0', RLCA, True),
    # The Old Line: into the cart; the ride (the broken bridge: the feather's second jump); off at
    # the buffer, and up to the Lantern with all four, and out: the end of the game.
    ('station', 'S', 'cart', 'none', True),
    ('station', 'S', 'gem 0', R, False),
    ('station', 'S', 'gem 0', RL, True),
    ('old_line', 'S', 'R', ALL, True, RIDING),
    ('broken_bridge', 'S', 'R', RLCA, False, RIDING),
    ('broken_bridge', 'S', 'R', ALL, True, RIDING),
    ('summit', 'S', 'stop', ALL, True, RIDING),
    ('summit', '11 35', 'R', RLCA, False),
    ('summit', '11 35', 'R', 'rope,long_rope,air_throw,double_jump', False),
    ('summit', '11 35', 'R', 'rope,long_rope,wall_climb,double_jump', False),
    ('summit', '11 35', 'R', 'rope,wall_climb,air_throw,double_jump', False),
    ('summit', '11 35', 'R', ALL, True),
]


def run(route, bot):
    room, start, goal, abilities, expected = route[:5]
    args = [bot] + room.split() + ['--abilities', abilities, '--goal'] + goal.split() + (route[5] if len(route) > 5 else [])
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
            room, start, goal, abilities, expected = route[:5]
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
