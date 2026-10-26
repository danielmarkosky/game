"""Makes every room of the world and writes them into assets/world.ldtk.

  python3 tools/make_world.py                    # rebuild assets/world.ldtk
  python3 tools/make_world.py rooms/             # only write the rooms as text (see tools/rooms.py)
  python3 tools/make_world.py --export gully f   # write one room as the room editor's file f

The rooms are designed in tools/world/, a file per region (tools/world/build.py has what they're
made with), or by hand in the room editor (tools/editor.cpp), a file each in tools/world/rooms/. A
room in tools/world/rooms/ takes the place of one of the same name made in tools/world/: that's what
the editor does to edit one. Running this replaces the rooms in the LDtk project, so a room changed
in LDtk would be overwritten: it refuses to, if the project was changed since this last wrote it
(--force to anyway). Change the rooms in tools/world/ instead, then run tools/check_world.py.

Each room is drawn with y up (y = 0 is the bottom row), sized to the moves `levelbot --measure`
prints. Doors are gaps in the outside walls; JOINS says which door leads to which, and the rooms are
placed on the map so that joined doors line up, starting from the first room at (0, 0). The first
room is where a new game starts."""
import hashlib
import os
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
argv = sys.argv[1:]
EXPORT = None
if '--export' in argv:
    i = argv.index('--export')
    EXPORT = argv[i + 1:i + 3]
    del argv[i:i + 3]
args = [a for a in argv if a != '--force']
FORCE = '--force' in argv
OUT = args[0] if args else tempfile.mkdtemp()

from world.build import ROOMS  # noqa: E402
from world import hand  # noqa: E402
# The regions, in the order their rooms go in the project: a new game starts in the first room
# (the Hidden Lake). Then the rooms made by hand; and the room with the way out goes last.
import world.opening  # noqa: E402,F401
import world.secret  # noqa: E402,F401
import world.still  # noqa: E402,F401
import world.drowned  # noqa: E402,F401
import world.garden  # noqa: E402,F401
import world.hollows  # noqa: E402,F401
import world.frost  # noqa: E402,F401
import world.sky  # noqa: E402,F401
import world.line  # noqa: E402,F401

# ----------------------------------------------------------------------------------------------
# How they join: (room, side, which door of that side) to (room, side, which door).
# ----------------------------------------------------------------------------------------------

JOINS = [
    # The Hidden Lake, out of the cave onto the Old Road, the Gully, and the Campfire.
    ('lake', 'R', 0, 'old_road', 'L', 0),
    ('old_road', 'R', 0, 'gully', 'L', 0),
    ('gully', 'R', 0, 'camp', 'L', 0),
    # Behind the lake's waterfall, the wall that isn't there: on through it to the Still, and up the
    # crack, the way to the Hidden Spring. (The lake's left doors: 0 behind the falls, 1 at the top.)
    ('lake', 'L', 0, 'still', 'R', 0),
    ('still', 'L', 0, 'falls_source', 'R', 0),
    ('lake', 'L', 1, 'hidden_spring', 'R', 0),
    # Down the crack in the lake bed, the Drowned Grotto: for the diving suit.
    ('lake', 'B', 0, 'drowned', 'T', 0),
    # From the Campfire, the four routes, and the last. The Hook Garden east along the road, the
    # Hollows down the sinkhole: open from the start. The Frost Caves off the lookout on the left (for
    # the long rope, from the garden) and the Sky Road up through its top (for the chalk bag, from
    # the Hollows). Up the planks on the right, the Old Station: the cart up the Old Line, once all
    # four are done. (The camp's left doors: 0 from the Gully, 1 up on the lookout; its right ones: 0
    # the road, 1 the station.)
    ('camp', 'R', 0, 'hook_garden', 'L', 0),
    ('hook_garden', 'R', 0, 'mushroom_glade', 'L', 0),
    ('mushroom_glade', 'R', 0, 'ring_orchard', 'L', 0),
    ('camp', 'B', 0, 'hollows', 'T', 0),
    ('hollows', 'L', 0, 'dark_hollow', 'R', 0),
    ('dark_hollow', 'L', 0, 'pips_den', 'R', 0),
    ('camp', 'L', 1, 'frost_caves', 'R', 0),
    ('frost_caves', 'L', 0, 'icicle_hall', 'R', 0),
    ('icicle_hall', 'L', 0, 'frozen_falls', 'R', 0),
    ('camp', 'T', 0, 'sky_road', 'B', 0),
    ('sky_road', 'R', 0, 'updraft_cliffs', 'L', 0),
    ('updraft_cliffs', 'T', 0, 'sky_edge', 'B', 0),
    ('camp', 'R', 1, 'station', 'L', 0),
    ('station', 'R', 0, 'old_line', 'L', 0),
    ('old_line', 'R', 0, 'broken_bridge', 'L', 0),
    ('broken_bridge', 'R', 0, 'summit', 'L', 0),
]

# ----------------------------------------------------------------------------------------------
# The rooms made by hand in the room editor (tools/world/hand.py), each joined by where its doors
# start. One that isn't joined to the world yet (nor to a room that is) is left out of it for now.
# One with the name of a room made above takes its place, and its joins those of that room.
# ----------------------------------------------------------------------------------------------

SIDES = {'L': 'left', 'R': 'right', 'T': 'top', 'B': 'bottom'}
made_by_hand = {r.name: (r, joins) for r, joins in hand.rooms()}
hand_joins = [join for _, joins in made_by_hand.values() for join in joins]
replaced = [name for name in made_by_hand if name in ROOMS]
for name in replaced:
    print(f'{name}: tools/world/rooms/{name}.txt is used, not the one made in tools/world/')
JOINS[:] = [join for join in JOINS if join[0] not in replaced and join[3] not in replaced]
for ra, _, _, rb, _, _ in hand_joins:
    if rb not in ROOMS and rb not in made_by_hand:
        sys.exit(f'tools/world/rooms/{ra}.txt is joined to {rb}, and there\'s no room called that')
in_world = set(ROOMS)
growing = True
while growing:
    growing = False
    for ra, _, _, rb, _, _ in hand_joins:
        for here, there in ((ra, rb), (rb, ra)):
            if here in in_world and there not in in_world:
                in_world.add(there)
                growing = True
for name, (r, _) in made_by_hand.items():
    if name in in_world:
        if hand.problem(r):
            sys.exit(f'{name} is in the world, but {hand.problem(r)} (tools/world/rooms/{name}.txt)')
        ROOMS[name] = r
    else:
        print(f'{name} (made by hand) isn\'t joined to the world yet, so it\'s left out of it')


def door_index(room, side, start):
    for i, (s, _) in enumerate(ROOMS[room].doors.get(side, [])):
        if s == start:
            return i
    sys.exit(f'{room} has no door in its {SIDES[side]} wall starting at {start}, for a join to it')


joined = set()
for ra, sa, a, rb, sb, b in hand_joins:
    if ra not in ROOMS or ((rb, sb, b), (ra, sa, a)) in joined:
        continue
    joined.add(((ra, sa, a), (rb, sb, b)))
    JOINS.append((ra, sa, door_index(ra, sa, a), rb, sb, door_index(rb, sb, b)))

# A door leads into one other at most.
used = set()
for ra, sa, ia, rb, sb, ib in JOINS:
    for room, side, i in ((ra, sa, ia), (rb, sb, ib)):
        if (room, side, i) in used:
            sys.exit(f'{room}\'s {SIDES[side]} door starting at {ROOMS[room].doors[side][i][0]} is joined twice')
        used.add((room, side, i))


def room_file(name):
    """A room as the room editor's file (see src/RoomFile.h): its joins by where the doors start."""
    r = ROOMS[name]
    lines = [f'title {r.title}', f'theme {r.theme}']
    for key in ('region', 'wind', 'dark', 'space'):
        if r.settings.get(key) not in (None, '', 0, '0', False):
            lines.append(f"{key} {'1' if key in ('dark', 'space') else r.settings[key]}")
    for ra, sa, ia, rb, sb, ib in JOINS:
        for here, side, i, there, other_side, j in ((ra, sa, ia, rb, sb, ib), (rb, sb, ib, ra, sa, ia)):
            if here == name:
                lines.append(f'join {side} {ROOMS[here].doors[side][i][0]} {there} {other_side} {ROOMS[there].doors[other_side][j][0]}')
    for (x, y), fields in sorted(r.fields.items(), key=lambda kv: (-kv[0][1], kv[0][0])):
        lines.append(f'entity {x} {y} ' + ' '.join(f'{k}={v}' for k, v in fields.items()))
    lines.append('tiles')
    lines += [''.join(row) for row in reversed(r.g)]
    return '\n'.join(lines) + '\n'


if EXPORT:
    if len(EXPORT) != 2 or EXPORT[0] not in ROOMS:
        sys.exit(f'--export <room> <file>: the rooms are {", ".join(ROOMS)}')
    with open(EXPORT[1], 'w') as f:
        f.write(room_file(EXPORT[0]))
    sys.exit(0)

# The ways out at the end of the game are the doors left unjoined (but for ones in a floor: those are
# pits). They all have to be in one room, which goes last.
ORDER = list(ROOMS)
assert ORDER[0] == 'lake', ORDER[0]
ways_out = [(name, side, start) for name in ORDER for side, ds in ROOMS[name].doors.items() if side != 'B'
            for i, (start, _) in enumerate(ds) if (name, side, i) not in used]
last = list(dict.fromkeys(name for name, _, _ in ways_out))
if len(last) != 1:
    listed = ', '.join(f"{name}'s {SIDES[side]} door at {start}" for name, side, start in ways_out)
    sys.exit('The world needs a way out for the end of the game: leave a door joined to nothing.' if not ways_out else
             f'The doors joined to nothing (the ways out, at the end of the game) have to be in one room, the last. They\'re in '
             f'{len(last)}: {listed}. Join or close all but one room\'s.')
ORDER.remove(last[0])
ORDER.append(last[0])


pos = {ORDER[0]: (0, 0)}
changed = True
while changed:
    changed = False
    for ra, sa, ia, rb, sb, ib in JOINS:
        A, B = ROOMS[ra], ROOMS[rb]
        da, db = A.doors[sa][ia], B.doors[sb][ib]
        assert da[1] == db[1], (ra, rb, 'doors of different sizes')
        assert {sa, sb} in ({'L', 'R'}, {'T', 'B'}), (ra, rb)
        for known, other, flip in ((ra, rb, False), (rb, ra, True)):
            if known in pos and other not in pos:
                (x, y) = pos[known]
                P, Q = (A, B) if not flip else (B, A)
                dp, dq = (da, db) if not flip else (db, da)
                sp = sa if not flip else sb
                if sp == 'R':   pos[other] = (x + P.w, y + dp[0] - dq[0])
                elif sp == 'L': pos[other] = (x - Q.w, y + dp[0] - dq[0])
                elif sp == 'T': pos[other] = (x + dp[0] - dq[0], y + P.h)
                elif sp == 'B': pos[other] = (x + dp[0] - dq[0], y - Q.h)
                changed = True
    # A join between two rooms that are both placed has to line up.
for ra, sa, ia, rb, sb, ib in JOINS:
    A, B = ROOMS[ra], ROOMS[rb]
    (ax, ay), (bx, by) = pos[ra], pos[rb]
    da, db = A.doors[sa][ia], B.doors[sb][ib]
    if sa in 'LR':
        assert ay + da[0] == by + db[0], (ra, rb, 'out of line', ay + da[0], by + db[0])
        assert (bx == ax + A.w) if sa == 'R' else (ax == bx + B.w), (ra, rb)
    else:
        assert ax + da[0] == bx + db[0], (ra, rb, 'out of line', ax + da[0], bx + db[0])
        assert (by == ay + A.h) if sa == 'T' else (ay == by + B.h), (ra, rb)

missing = [n for n in ROOMS if n not in pos]
assert not missing, missing
# Nothing overlapping.
names = list(ROOMS)
overlaps = []
for i, n in enumerate(names):
    for m in names[i + 1:]:
        (x1, y1), (x2, y2) = pos[n], pos[m]
        A, B = ROOMS[n], ROOMS[m]
        if x1 < x2 + B.w and x2 < x1 + A.w and y1 < y2 + B.h and y2 < y1 + A.h:
            overlaps.append(f'  {n} (tiles {x1}..{x1 + A.w - 1} across, {y1}..{y1 + A.h - 1} up) and {m} '
                            f'({x2}..{x2 + B.w - 1}, {y2}..{y2 + B.h - 1})')
if overlaps:
    sys.exit('Rooms overlap: move a door, or make one smaller.\n' + '\n'.join(overlaps))

minx = min(x for x, _ in pos.values())
miny = min(y for _, y in pos.values())
os.makedirs(OUT, exist_ok=True)
lines = ['# Rooms made by tools/make_world.py, for tools/rooms.py.']
theme = None
for name in ORDER:
    r = ROOMS[name]
    if r.theme != theme:
        lines.append(f'theme {r.theme}')
        theme = r.theme
    x, y = pos[name]
    lines.append(f'{name:<16} {x - minx:4d} {y - miny:4d}  {r.title}')
    with open(os.path.join(OUT, name + '.txt'), 'w') as f:
        f.write(r.text())
for name in ORDER:
    if ROOMS[name].settings:
        lines.append(f'room {name} ' + ' '.join(f'{k}={v}' for k, v in ROOMS[name].settings.items()))
for name in ORDER:
    r = ROOMS[name]
    for (x, y), fields in sorted(r.fields.items()):
        values = ' '.join(f'{k}={v}' for k, v in fields.items())
        lines.append(f'entity {name} {x} {r.h - 1 - y} {values}')
with open(os.path.join(OUT, 'rooms.txt'), 'w') as f:
    f.write('\n'.join(lines) + '\n')
if args:
    print(f'wrote {len(ROOMS)} rooms to {OUT}')
else:
    project = os.path.join(ROOT, 'assets', 'world.ldtk')
    # What the project was when this last wrote it: if it's changed since, someone changed rooms in
    # LDtk, and writing over it would lose that.
    written = os.path.join(ROOT, 'tools', 'world', 'written.sha256')

    def fingerprint():
        with open(project, 'rb') as f:
            return hashlib.sha256(f.read()).hexdigest()

    if os.path.exists(project) and os.path.exists(written) and not FORCE:
        with open(written) as f:
            expected = f.read().split()[0]
        if fingerprint() != expected:
            sys.exit('assets/world.ldtk was changed since tools/make_world.py last wrote it (in LDtk?), and\n'
                     'writing the rooms would lose that. Move the change into tools/world/ (see\n'
                     '`python3 tools/rooms.py export assets/world.ldtk <folder>` to look at it as text),\n'
                     'or run with --force to write over it anyway.')
    subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'rooms.py'), 'import', os.path.join(OUT, 'rooms.txt'), project], check=True)
    with open(written, 'w') as f:
        f.write(fingerprint() + '  assets/world.ldtk, as tools/make_world.py last wrote it\n')
    print(f'wrote {len(ROOMS)} rooms to {os.path.relpath(project, ROOT)}')
