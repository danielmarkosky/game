"""Makes every room of the world and writes them into assets/world.ldtk.

  python3 tools/make_world.py              # rebuild assets/world.ldtk
  python3 tools/make_world.py rooms/       # only write the rooms as text (see tools/rooms.py)

This script is where the rooms are designed: running it replaces the rooms in the LDtk project, so a
room changed in LDtk would be overwritten. Change it here instead, then run tools/check_world.py.

Each room is drawn with y up (y = 0 is the bottom row), sized to the moves `levelbot --measure`
prints. Doors are gaps in the outside walls; JOINS says which door leads to which, and the rooms are
placed on the map so that joined doors line up, starting from the first room at (0, 0). The first
room is where a new game starts."""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
OUT = sys.argv[1] if len(sys.argv) > 1 else tempfile.mkdtemp()


class Room:
    def __init__(self, name, title, theme, w, h):
        self.name, self.title, self.theme, self.w, self.h = name, title, theme, w, h
        self.g = [['.'] * w for _ in range(h)]
        self.doors = {}   # side -> [(start, length)]
        self.fields = {}  # (x, y) -> {field: value}
        self.settings = {}  # wind, dark: see tools/rooms.py
        self.fill(0, w - 1, 0, 0, '#')
        self.fill(0, w - 1, h - 1, h - 1, '#')
        self.fill(0, 0, 0, h - 1, '#')
        self.fill(w - 1, w - 1, 0, h - 1, '#')

    def fill(self, x0, x1, y0, y1, c):
        for x in range(min(x0, x1), max(x0, x1) + 1):
            for y in range(min(y0, y1), max(y0, y1) + 1):
                self.g[y][x] = c
        return self

    def put(self, x, y, c, **fields):
        self.g[y][x] = c
        if fields:
            self.fields[(x, y)] = fields
        return self

    def prop(self, x, y, kind):
        """Scenery (a prop in assets/gfx/props.txt, or light_shaft), in an empty tile: standing on
        its bottom, or hanging from its top."""
        assert self.g[y][x] == '.', (self.name, x, y, kind, 'not empty')
        return self.put(x, y, 'D', kind=kind)

    def props(self, *placed):
        for x, y, kind in placed:
            self.prop(x, y, kind)
        return self

    def door(self, side, start, length=4):
        """A gap in an outside wall: L and R count `start` up from the bottom, T and B from the left."""
        for i in range(length):
            if side == 'L': self.g[start + i][0] = '.'
            if side == 'R': self.g[start + i][self.w - 1] = '.'
            if side == 'B': self.g[0][start + i] = '.'
            if side == 'T': self.g[self.h - 1][start + i] = '.'
        self.doors.setdefault(side, []).append((start, length))
        return self

    def text(self):
        return '\n'.join(''.join(self.g[y]) for y in range(self.h - 1, -1, -1)) + '\n'


ROOMS = {}


def room(*args):
    r = Room(*args)
    ROOMS[r.name] = r
    return r


def ground(r, top, x0=0, x1=None):
    """Rock from the bottom up to `top`: stand at top + 1."""
    return r.fill(x0, r.w - 1 if x1 is None else x1, 0, top, '#')


def roof(r, bottom, x0=0, x1=None):
    return r.fill(x0, r.w - 1 if x1 is None else x1, bottom, r.h - 1, '#')


def pit(r, x0, x1, bottom=1, top=4, spikes=True):
    r.fill(x0, x1, bottom, top, '.')
    if spikes:
        r.fill(x0, x1, bottom, bottom, '^')
    return r


# ----------------------------------------------------------------------------------------------
# The Crevasse: where a new game starts. Walking, jumping, Ash's rope, the first swings.
# ----------------------------------------------------------------------------------------------

a = room('crevasse_wake', 'The Crevasse', 'cave', 40, 22)
ground(a, 4); roof(a, 18)
a.fill(17, 19, 15, 18, '.').fill(18, 18, 19, 20, '.')   # the fissure you fell through, far above
a.fill(0, 4, 5, 17, '#').fill(5, 6, 9, 17, '#')          # the back wall
a.fill(26, 27, 5, 5, '#').fill(31, 31, 5, 5, '#')         # rubble
a.fill(36, 39, 9, 17, '#')
a.door('R', 5)
a.put(9, 5, 'S')

b = room('crevasse_steps', 'The Crevasse', 'cave', 40, 22)
ground(b, 4); roof(b, 17)
b.fill(10, 12, 5, 5, '#').fill(16, 18, 5, 6, '#').fill(22, 24, 5, 7, '#')
b.fill(27, 39, 5, 7, '#').fill(30, 39, 12, 16, '#')
b.door('L', 5).door('R', 8)
b.put(3, 5, 'S')

c = room('crevasse_gaps', 'The Crevasse', 'cave', 44, 22)
ground(c, 4); roof(c, 16)
pit(c, 10, 11, 2, 4, spikes=False)
pit(c, 16, 18, 2, 4, spikes=False)
pit(c, 26, 29)
c.put(22, 5, 'C')
c.door('L', 5).door('R', 5)
c.put(3, 5, 'S')

d = room('crevasse_camp', 'The Crevasse', 'cave', 48, 24)
ground(d, 4)
d.fill(0, 13, 13, 23, '#').fill(14, 47, 19, 23, '#')
d.put(9, 5, 'P', ability='rope')                          # Ash's rope, in her old camp
d.put(15, 5, 'N', dialogue='note_crevasse', look='note')  # and a note, the first of hers
pit(d, 17, 23, 1, 4, spikes=False)
d.fill(17, 17, 1, 2, '#')                                  # a step back out
d.fill(24, 47, 5, 5, '#')                                  # the far side, a tile higher
d.fill(15, 27, 12, 13, '#').put(20, 12, '%').put(21, 12, '%')
d.door('L', 5).door('R', 6)
d.put(3, 5, 'S')

e = room('crevasse_dips', 'The Crevasse', 'cave', 44, 22)
ground(e, 4); roof(e, 17)
pit(e, 8, 16, 2, 4, spikes=False)
pit(e, 19, 27, 2, 4, spikes=False)
e.fill(6, 30, 10, 11, '#')
for x in (12, 13, 23, 24):
    e.put(x, 10, '%')
e.put(18, 8, '*')
e.door('L', 5).door('R', 5)
e.put(3, 5, 'S')

f = room('crevasse_chasm', 'The Crevasse', 'cave', 52, 24)
ground(f, 4); roof(f, 19)
f.put(9, 5, 'C')
pit(f, 13, 24)
f.fill(10, 28, 10, 11, '#')
for x in (16, 17, 21, 22):
    f.put(x, 10, '%')
f.door('L', 5).door('R', 5)
f.put(3, 5, 'S')

# ----------------------------------------------------------------------------------------------
# The Cave and The Well: up to the Crossroads. The rope, thrown standing.
# ----------------------------------------------------------------------------------------------

cave = room('level1', 'The Cave', 'cave', 48, 20)
ground(cave, 4); roof(cave, 16)
cave.put(2, 5, 'B').put(12, 5, 'S')
cave.put(7, 5, 'N', dialogue='wren', look='wren')
cave.fill(22, 30, 5, 9, '#')                                # a shelf, too high to jump onto
cave.fill(21, 21, 13, 15, '#').put(21, 12, '%')            # hook blocks hanging from the roof
cave.fill(31, 31, 13, 15, '#').put(31, 12, '%')
cave.fill(38, 38, 13, 15, '#').put(38, 12, '%')
cave.put(43, 10, '*')                                       # something shiny, up under the roof
cave.door('L', 5).door('R', 5)

well = room('level2', 'The Well', 'cave', 26, 38)
ground(well, 4)
well.door('L', 5)
# Shelves up the shaft, one side then the other, a hook block out from each one's edge.
shelves = [(15, 24, 9), (1, 10, 14), (15, 24, 19), (1, 10, 24), (15, 24, 29), (1, 10, 33)]
for x0, x1, top in shelves:
    well.fill(x0, x1, top - 1, top, '#')
hooks = [(13, 12), (12, 17), (13, 22), (12, 27), (13, 32), (12, 36)]
for x, y in hooks:
    well.put(x, y, '%')
well.put(20, 22, '*')
well.door('T', 3)
well.fill(20, 23, 1, 4, '.').fill(20, 23, 1, 1, '-').door('B', 20)  # down into The Deep, and a step to come back up onto
well.put(3, 5, 'S')

# ----------------------------------------------------------------------------------------------
# The Crossroads: the hub. Pip and a bench; the ruins west, the garden east, the Tall Hall up.
# ----------------------------------------------------------------------------------------------

hub = room('crossroads', 'The Crossroads', 'garden', 60, 32)
ground(hub, 4)
hub.door('B', 8)
hub.fill(8, 11, 1, 4, '.').fill(8, 11, 1, 1, '-')            # up out of the well onto this
hub.put(18, 5, 'B').put(14, 5, 'S')
hub.put(26, 5, 'N', dialogue='pip', look='pip')
hub.door('L', 5).door('R', 5)
# Up the middle to the Tall Hall: shelves one side then the other, hook blocks out from their edges.
for x0, x1, top in [(32, 40, 9), (16, 24, 14), (32, 40, 19), (16, 24, 24), (24, 33, 28)]:
    hub.fill(x0, x1, top - 1, top, '#')
for x, y in [(30, 12), (26, 17), (30, 22), (26, 27)]:
    hub.put(x, y, '%')
hub.door('T', 26)   # the Tall Hall
hub.door('T', 44)   # where the Drop comes down, too high to go back up
hub.put(10, 20, '*')
hub.put(28, 5, 'C')

# ----------------------------------------------------------------------------------------------
# The ruins, west: down to the Archive, where Moss keeps Ash's chalk bag.
# ----------------------------------------------------------------------------------------------

gate = room('ruins_gate', 'The Sunken Gate', 'ruins', 48, 24)
ground(gate, 4); roof(gate, 18)
pit(gate, 14, 22)
gate.fill(16, 16, 12, 17, '#').put(16, 11, '%')
gate.fill(21, 21, 12, 17, '#').put(21, 11, '%')
gate.fill(29, 37, 5, 9, '#')                                 # a block of old wall, too high to jump
gate.fill(28, 28, 13, 17, '#').put(28, 12, '%')
gate.fill(38, 38, 13, 17, '#').put(38, 12, '%')
gate.put(33, 14, '*')
gate.door('L', 5).door('R', 5)
gate.put(44, 5, 'S')

hollow = room('ruins_hollow', 'Mushroom Hollow', 'ruins', 44, 34)
ground(hollow, 4)
hollow.door('R', 5)
# Up from the floor, shelf by shelf, to a high way on to the Archive.
hollow.fill(24, 34, 8, 9, '#').fill(10, 20, 13, 14, '#').fill(24, 34, 18, 19, '#').fill(1, 12, 1, 24, '#')
for x, y in [(22, 12), (22, 17), (22, 22), (14, 27)]:
    hollow.put(x, y, '%')
hollow.put(29, 25, '*')
hollow.door('L', 25)
hollow.put(40, 5, 'S')

archive = room('ruins_archive', 'The Archive', 'ruins', 40, 28)
ground(archive, 4)
archive.door('R', 5)
archive.put(4, 5, 'B').put(9, 5, 'S')
archive.put(15, 5, 'N', dialogue='moss', look='moss')
# The shelves, up to the top one and the chalk bag.
for x0, x1, top in [(22, 34, 9), (4, 16, 14), (22, 34, 19), (6, 14, 23)]:
    archive.fill(x0, x1, top - 1, top, '#')
for x, y in [(20, 12), (19, 17), (20, 22), (17, 26)]:
    archive.put(x, y, '%')
archive.put(9, 24, 'P', ability='wall_climb')
archive.put(28, 10, 'N', dialogue='note_ruins', look='note')
archive.put(30, 21, '*')

# ----------------------------------------------------------------------------------------------
# The garden, east: hook blocks over spikes, and the long rope at its end.
# ----------------------------------------------------------------------------------------------

garden = room('level3', 'Hook Garden', 'garden', 60, 24)
ground(garden, 4); roof(garden, 19)
pit(garden, 10, 19)
garden.fill(10, 19, 13, 18, '#')
for x in (13, 16):
    garden.put(x, 12, '%')
pit(garden, 23, 31)
garden.fill(23, 31, 13, 18, '#').put(27, 12, '%')
garden.fill(42, 48, 5, 9, '#')
garden.fill(41, 41, 13, 18, '#').put(41, 12, '%')
garden.fill(49, 49, 13, 18, '#').put(49, 12, '%')
garden.put(27, 9, '*')
garden.put(37, 5, 'C')
garden.door('L', 5).door('R', 5)
garden.put(3, 5, 'S')

curtains = room('level4', 'Curtains', 'garden', 56, 28)
ground(curtains, 4)
curtains.door('L', 5)
curtains.fill(16, 41, 5, 9, '#')                               # a long terrace: stand at 10
curtains.fill(14, 14, 13, 27, '#').put(14, 12, '%')
curtains.fill(42, 55, 0, 4, '#')
# The long rope, down in a slick-walled hollow: in by dropping, out only with a longer rope.
curtains.fill(42, 43, 5, 10, 'o').fill(54, 54, 1, 27, 'o')
curtains.fill(44, 53, 1, 4, '#')
curtains.put(50, 5, 'P', ability='long_rope')
curtains.put(47, 5, 'F', race='garden')                       # where Bramble's race ends
curtains.fill(42, 55, 20, 27, 'o').fill(48, 48, 18, 19, 'o').put(48, 17, '%')
curtains.put(28, 13, '*')
curtains.put(3, 5, 'S')

# ----------------------------------------------------------------------------------------------
# Up through the Tall Hall to the mountains. The long rope, then climbing, then throwing in the air.
# ----------------------------------------------------------------------------------------------

hall = room('tall_hall', 'The Tall Hall', 'snow', 22, 35)
hall.fill(0, 21, 0, 2, '#').door('B', 9)
hall.fill(9, 12, 1, 2, '.').fill(9, 12, 1, 1, '-')
hall.fill(1, 1, 3, 33, 'o').fill(20, 20, 3, 33, 'o')         # crystal-smooth walls: nothing to hold
# Ledges of the same, one side then the other, each 7 up from the last. Each one's hook is 9 up:
# a tile beyond a short rope.
for x0, x1, top, hx, hy in [(14, 19, 9, 12, 12), (2, 7, 16, 9, 19), (14, 19, 23, 12, 26), (2, 7, 30, 9, 33)]:
    hall.fill(x0, x1, top - 1, top, 'o').put(hx, hy, '%')
hall.door('T', 3)
hall.put(17, 10, '*')
hall.put(10, 3, 'S')

drop = room('the_drop', 'The Drop', 'snow', 8, 35)
drop.fill(1, 1, 1, 33, 'o').fill(6, 6, 1, 33, 'o')
drop.door('T', 2).door('B', 2)
drop.put(3, 28, 'S')

summit = room('level5', 'The Summit', 'snow', 64, 34)
ground(summit, 4)
summit.door('B', 3)
summit.fill(3, 6, 1, 4, '.').fill(3, 6, 1, 1, '-')
summit.put(26, 5, 'S').put(14, 5, 'B')
summit.put(20, 5, 'N', dialogue='juniper', look='juniper')
summit.put(24, 5, 'L')
summit.fill(27, 30, 1, 3, '.').fill(27, 30, 4, 4, '=').door('B', 27)  # the gate over the Drop
# A narrow chimney up to the throwing hand: climb it, throw from the wall.
# A chimney, rock inside and slick outside, to a gem at the top: leap from wall to wall.
summit.fill(34, 34, 8, 21, 'o').fill(35, 35, 8, 21, '#').fill(39, 39, 8, 21, '#').fill(40, 40, 8, 21, 'o')
summit.fill(34, 40, 22, 33, '#')
summit.put(37, 19, '*')
# West, the Frost Caves: up a short wall only hands can climb (its top is slick), to a door.
summit.fill(1, 10, 13, 13, 'o').fill(10, 10, 8, 12, '#').door('L', 14)
# The way on: up a slick cliff, by a hook too high to throw to from the ground, but not from the top
# of a jump.
summit.fill(50, 62, 1, 15, 'o')
summit.put(47, 19, '%').put(48, 19, '%')
summit.fill(42, 62, 24, 32, 'o')
summit.door('R', 16)
summit.put(30, 9, '*')

stones = room('level6', 'Stepping Stones', 'snow', 60, 28)
ground(stones, 4)
stones.fill(1, 5, 5, 15, '#')                                 # in high on the left
stones.door('L', 16)
pit(stones, 6, 53)
for x in (14, 24, 34, 44):
    stones.fill(x, x + 1, 1, 6, '#')
for x, y in [(10, 14), (19, 15), (29, 14), (39, 15), (49, 14), (8, 18)]:
    stones.put(x, y, '%')
stones.fill(0, 59, 24, 27, 'o')
stones.put(29, 9, '*')
stones.door('R', 5)
stones.put(3, 16, 'S')

chimney = room('level7', 'The Chimney', 'ember', 18, 46)
ground(chimney, 4)
chimney.door('L', 5)
chimney.fill(1, 6, 9, 44, '#').fill(11, 16, 5, 44, '#')      # rock walls, a narrow shaft between
# Alcoves cut into the walls, to climb over into and rest in.
for x0, x1, y in [(4, 6, 13), (11, 13, 21), (4, 6, 29), (11, 13, 37)]:
    chimney.fill(x0, x1, y, y + 3, '.')
chimney.put(12, 22, '*')
chimney.put(13, 37, 'F', race='chimney')                      # where Cinder's race ends
chimney.door('T', 7)
chimney.put(3, 5, 'S')

pendulums = room('level8', 'Pendulums', 'ember', 60, 26)
ground(pendulums, 4)
pendulums.door('B', 4)
pendulums.fill(4, 7, 1, 4, '.').fill(4, 7, 1, 1, '-')
pendulums.put(10, 5, 'B').put(14, 5, 'S')
pit(pendulums, 18, 53)
for x in (26, 35, 44):
    pendulums.fill(x, x + 1, 1, 5, '#')                       # pillars to land on between the swings
for x in (22, 31, 40, 49):
    pendulums.fill(x, x, 13, 25, '#').put(x, 12, '%')         # hook blocks hanging from the roof
pendulums.put(21, 12, '%')                                    # the first one halfway from the ledge
pendulums.fill(54, 59, 0, 4, '#')
pendulums.put(35, 9, '*')
pendulums.door('R', 5)

backtrack = room('level9', 'Backtrack', 'sky', 52, 30)
backtrack.settings['wind'] = '40'                             # out in the wind now, at your back
ground(backtrack, 4)
backtrack.door('L', 5)
backtrack.fill(30, 50, 5, 14, '#')
backtrack.fill(30, 30, 5, 14, '#')
backtrack.fill(22, 22, 18, 29, '#').put(22, 17, '%')
backtrack.put(44, 15, '*')
backtrack.door('R', 15)
backtrack.put(22, 12, '*')
backtrack.put(4, 5, 'S')

windy = room('windy_ledge', 'The Windy Ledge', 'sky', 56, 30)
windy.fill(0, 55, 0, 14, 'o')
windy.door('L', 15)
pit(windy, 12, 17, 1, 14)
pit(windy, 29, 35, 1, 14)
windy.fill(0, 55, 24, 29, 'o')
windy.put(32, 19, '*')
windy.door('R', 15)
windy.put(3, 15, 'S')

daylight = room('level10', 'Daylight', 'daylight', 44, 34)
ground(daylight, 4)
daylight.door('L', 15)
daylight.fill(1, 10, 5, 14, '#')
daylight.put(36, 23, 'N', dialogue='ash', look='ash')
for x0, x1, top in [(30, 42, 10), (14, 24, 16), (30, 42, 22), (14, 24, 28)]:
    daylight.fill(x0, x1, top - 1, top, '#')
for x, y in [(27, 13), (27, 19), (27, 25), (27, 31)]:
    daylight.put(x, y, '%')
daylight.door('T', 18)
daylight.put(6, 15, 'S')


# ----------------------------------------------------------------------------------------------
# The Deep, below the Well: a challenge. Crumbling stones over spikes to Wren's lost lantern, and a
# ladder of shelves back up.
# ----------------------------------------------------------------------------------------------

deep = room('the_deep', 'The Deep', 'cave', 40, 30)
ground(deep, 4)
deep.door('T', 2)
for x0, x1, top in [(12, 18, 9), (1, 8, 14), (12, 18, 19), (1, 8, 25)]:
    deep.fill(x0, x1, top - 1, top, '#')
for x, y in [(10, 12), (10, 17), (10, 22), (10, 28)]:
    deep.put(x, y, '%')
pit(deep, 21, 32)
for x0, y in [(21, 10), (25, 11), (29, 12)]:
    deep.fill(x0, x0 + 1, y, y, '&')
deep.fill(33, 38, 1, 12, '#')
deep.put(36, 13, 'I', item='wren_lantern')
deep.put(28, 16, '%')
deep.put(23, 14, '*').put(31, 16, '*').put(34, 13, '*')
deep.put(4, 5, 'S')

# ----------------------------------------------------------------------------------------------
# The Market, between the Crossroads and the garden: Tuck's stall.
# ----------------------------------------------------------------------------------------------

market = room('market', 'The Market', 'garden', 40, 22)
ground(market, 4)
market.door('L', 5).door('R', 5)
market.put(14, 5, 'B').put(24, 5, 'N', dialogue='tuck', look='tuck')
market.fill(28, 33, 5, 7, '#')
market.put(31, 8, '*')
market.put(3, 5, 'S')

# ----------------------------------------------------------------------------------------------
# More of the ruins: the Lever Works (a gate the rope opens, through a slit), the Dark Stair (lamps
# the rope lights), and the Vault, up a chimney only hands can climb.
# ----------------------------------------------------------------------------------------------

works = room('lever_works', 'The Lever Works', 'ruins', 48, 26)
ground(works, 4); roof(works, 18)
works.door('L', 5).door('R', 5)
works.fill(20, 21, 5, 17, '=').put(20, 6, '.').put(21, 6, '.')  # a gate with a slit the rope fits through, you don't
works.fill(17, 18, 5, 5, '#').put(18, 6, 'L')                    # the lever behind it, on a step
works.fill(23, 27, 5, 5, '#')                                     # a step to throw from, on this side
works.fill(40, 40, 8, 25, '#').fill(44, 44, 8, 25, '#').fill(41, 43, 18, 25, '.').door('T', 41, 3)  # the chimney up to the Vault
works.fill(41, 43, 13, 13, '-').fill(41, 43, 19, 19, '-')  # ledges to rest on: the grip only lasts so long
works.put(8, 8, '*')
works.put(44, 5, 'S')

stair = room('dark_stair', 'The Dark Stair', 'ruins', 40, 34)
stair.settings['dark'] = '1'
ground(stair, 4)
stair.fill(28, 38, 1, 24, '#').door('R', 25).door('L', 5)
for x0, x1, top in [(15, 24, 9), (1, 10, 14), (15, 24, 19)]:
    stair.fill(x0, x1, top - 1, top, '#')
pit(stair, 25, 27)
for x, y in [(13, 12), (12, 17), (13, 22), (26, 27)]:
    stair.put(x, y, '%')
for x, y in [(5, 12), (20, 13), (5, 22), (20, 25), (33, 30)]:
    stair.put(x, y, 'Q')
stair.put(8, 18, '*')
stair.put(34, 25, 'S')

vault = room('the_vault', 'The Vault', 'ruins', 36, 24)
vault.settings['dark'] = '1'
ground(vault, 4)
vault.door('B', 1, 3).fill(1, 3, 1, 4, '.').fill(1, 3, 1, 1, '-')
pit(vault, 11, 30)
for x0, y in [(13, 5), (18, 6), (23, 7)]:
    vault.fill(x0, x0 + 1, 1, y, '&')
vault.fill(28, 34, 1, 7, '#')
vault.put(20, 15, '%')
for x, y in [(16, 14), (26, 14), (6, 12)]:
    vault.put(x, y, 'Q')
vault.put(31, 8, '*').put(20, 11, '*').put(33, 8, '*')
vault.put(7, 5, 'S')

# ----------------------------------------------------------------------------------------------
# More of the garden: rings moving on tracks, the Greenhouse (Bramble, a bench, a race), the
# Trellis (a ring that lifts you), and the Thorn Walk above, a challenge.
# ----------------------------------------------------------------------------------------------

rings = room('garden_rings', 'The Ring Garden', 'garden', 48, 26)
ground(rings, 4); roof(rings, 20)
rings.door('L', 5).door('R', 5)
pit(rings, 8, 38)
rings.fill(21, 24, 1, 5, '#')
rings.put(12, 11, 'R', to='6,0', period='4').put(28, 11, 'R', to='6,0', period='5')
rings.put(23, 10, '*')
rings.put(3, 5, 'S')

green = room('greenhouse', 'The Greenhouse', 'garden', 40, 24)
ground(green, 4)
green.door('L', 5).door('R', 5)
green.put(8, 5, 'B').put(14, 5, 'N', dialogue='bramble', look='bramble')
green.put(20, 5, 'N', dialogue='note_garden', look='note')
for x0, x1, top in [(24, 30, 9), (32, 38, 14), (24, 30, 19)]:
    green.fill(x0, x1, top - 1, top, '#')
for x, y in [(22, 12), (30, 17), (32, 22)]:
    green.put(x, y, '%')
green.door('T', 25)
green.put(3, 5, 'S')

trellis = room('trellis', 'The Trellis', 'garden', 48, 26)
ground(trellis, 4)
trellis.door('L', 5).door('R', 5)
trellis.fill(22, 25, 5, 11, 'o')                              # too high to jump, and the rope slides off it
trellis.put(18, 7, 'R', to='0,12', period='5').put(29, 7, 'R', to='0,12', period='5')  # rings that lift you
trellis.put(23, 14, '*')
trellis.put(3, 5, 'S')

thorn = room('thorn_walk', 'The Thorn Walk', 'garden', 40, 26)
ground(thorn, 4)
thorn.door('B', 25).fill(25, 28, 1, 4, '.').fill(25, 28, 1, 1, '-')
pit(thorn, 4, 22)
thorn.fill(1, 3, 1, 8, '#')
thorn.put(19, 10, 'R', to='-4,0', period='3').put(12, 11, 'R', to='0,4', period='3.5').put(6, 12, 'R', to='-3,0', period='3')
thorn.put(17, 8, '*').put(11, 13, '*').put(2, 9, '*')
thorn.put(34, 5, 'S')

# ----------------------------------------------------------------------------------------------
# The Frost Caves, west of the Summit: ice to slide on, icicles that fall. Pell's camp, and at
# the end Ash's frozen camp, with her throwing glove.
# ----------------------------------------------------------------------------------------------

icefall = room('icefall', 'The Icefall', 'ice', 44, 28)
ground(icefall, 4); roof(icefall, 22)
icefall.fill(34, 43, 1, 13, '#').door('R', 14).door('L', 5)
for i, x0 in enumerate((30, 26, 22, 18)):
    icefall.fill(x0, x0 + 3, 1, 12 - 2 * i, '~')
icefall.fill(1, 17, 4, 4, '~')
icefall.put(16, 21, 'Y').put(24, 21, 'Y')
icefall.put(6, 8, '*')
icefall.put(40, 14, 'S')

lake = room('frozen_lake', 'The Frozen Lake', 'ice', 56, 24)
ground(lake, 4); roof(lake, 18)
lake.door('R', 5).door('L', 5)
pit(lake, 9, 46)
lake.fill(1, 8, 4, 4, '~').fill(47, 54, 4, 4, '~')
for x0, x1 in [(13, 17), (22, 26), (31, 35), (40, 43)]:
    lake.fill(x0, x1, 1, 3, '#').fill(x0, x1, 4, 4, '~')
for x in (15, 24, 33, 42):
    lake.put(x, 17, '%')
lake.put(29, 10, '*')
lake.put(52, 5, 'S')

fcamp = room('frost_camp', 'The Frost Camp', 'ice', 40, 24)
ground(fcamp, 4); roof(fcamp, 18)
fcamp.door('R', 5).door('L', 5)
fcamp.put(8, 5, 'B').put(16, 5, 'N', dialogue='pell', look='pell')
fcamp.fill(30, 30, 8, 23, '#').fill(34, 34, 8, 23, '#').fill(31, 33, 18, 23, '.').door('T', 31, 3)  # a chimney up
fcamp.fill(31, 33, 13, 13, '-').fill(31, 33, 18, 18, '-')
fcamp.put(36, 5, 'S')

gallery = room('icicle_gallery', 'The Icicle Gallery', 'ice', 52, 24)
ground(gallery, 4); roof(gallery, 16)
gallery.door('R', 5).door('L', 5)
for x in (10, 16, 22, 28, 34, 40):
    gallery.put(x, 15, 'Y')
for x0 in (13, 25, 37):
    pit(gallery, x0, x0 + 2)
gallery.fill(40, 46, 10, 11, '#').put(43, 15, '%')         # a shelf over the way, Moss's page on it
gallery.put(43, 12, 'I', item='moss_page')
gallery.put(30, 9, '*')
gallery.put(4, 5, 'S')

frozen = room('frozen_camp', "Ash's Frozen Camp", 'ice', 32, 22)
ground(frozen, 4); roof(frozen, 16)
frozen.door('R', 5)
frozen.put(10, 5, 'P', ability='air_throw')
frozen.put(16, 5, 'N', dialogue='note_frost', look='note')
frozen.put(28, 5, 'S')

icehall = room('hall_of_icicles', 'The Hall of Icicles', 'ice', 36, 26)
ground(icehall, 4); roof(icehall, 20)
icehall.door('B', 3, 3).fill(3, 5, 1, 4, '.').fill(3, 5, 1, 1, '-')
pit(icehall, 10, 30)
for x0, top in [(12, 6), (19, 7), (26, 8)]:
    icehall.fill(x0, x0 + 1, 1, top, '#').put(x0, 19, 'Y')
icehall.fill(31, 34, 1, 9, '#')
icehall.put(13, 12, '*').put(20, 13, '*').put(33, 10, '*')
icehall.put(8, 5, 'S')

# ----------------------------------------------------------------------------------------------
# The Ember Chimney, on from the Summit: the Forge (Cinder, a bench, a race up the Chimney), the
# Crumbling Halls, the Updraft (heat to ride, and the feather at the top), and the Furnace below.
# ----------------------------------------------------------------------------------------------

forge = room('forge', 'The Forge', 'ember', 40, 24)
ground(forge, 4); roof(forge, 18)
forge.door('L', 5).door('R', 5)
forge.put(8, 5, 'B').put(14, 5, 'N', dialogue='cinder', look='cinder')
forge.put(28, 5, 'N', dialogue='note_ember', look='note')
forge.fill(31, 37, 9, 10, '#').put(29, 13, '%').put(34, 11, '*')
forge.put(36, 5, 'S')

halls = room('crumbling_halls', 'The Crumbling Halls', 'ember', 56, 26)
ground(halls, 4); roof(halls, 20)
halls.door('L', 5).door('R', 5)
halls.door('B', 1).fill(1, 4, 1, 4, '.').fill(1, 4, 1, 1, '-')
pit(halls, 9, 46)
for x0, top in [(12, 5), (17, 6), (22, 7), (27, 7), (32, 6), (37, 5), (42, 5)]:
    halls.fill(x0, x0 + 1, 1, top - 1, '#').fill(x0, x0 + 1, top, top, '&')
halls.put(20, 13, '%').put(35, 13, '%')
halls.put(30, 12, '*')
halls.put(7, 5, 'S')

updraft = room('the_updraft', 'The Updraft', 'ember', 40, 40)
ground(updraft, 4)
updraft.door('L', 5).door('R', 30)
updraft.fill(30, 39, 1, 29, '#')
updraft.put(10, 5, 'U').put(20, 5, 'U')
updraft.fill(14, 18, 24, 25, '#').put(16, 26, 'P', ability='double_jump')
updraft.fill(1, 5, 32, 33, '#').put(3, 34, 'I', item='lucky_lure')
updraft.put(25, 20, '*')
updraft.put(4, 5, 'S')

furnace = room('furnace', 'The Furnace', 'ember', 40, 26)
ground(furnace, 4)
furnace.door('T', 1)
furnace.fill(1, 8, 1, 15, '#')
pit(furnace, 9, 37)
furnace.put(12, 5, 'U').put(26, 5, 'U')
for x0, top in [(17, 9), (22, 11), (31, 9)]:
    furnace.fill(x0, x0 + 1, top, top, '&')
furnace.fill(35, 38, 1, 10, '#')
furnace.put(13, 20, '*').put(27, 21, '*').put(37, 11, '*')
furnace.put(5, 16, 'S')

# ----------------------------------------------------------------------------------------------
# The Sky: clouds that fade, wind to fight, and the Last Ledge before the top. The Eye of the Storm
# above it, a challenge.
# ----------------------------------------------------------------------------------------------

clouds = room('cloud_steps', 'Cloud Steps', 'sky', 52, 30)
clouds.door('L', 15).door('R', 15)
clouds.fill(0, 7, 0, 14, '#').fill(44, 51, 0, 14, '#')
pit(clouds, 8, 43, 1, 1)
for x0, top in [(10, 14), (16, 16), (22, 18), (28, 16), (34, 14), (39, 15)]:
    clouds.fill(x0, x0 + 2, top, top, '_')
clouds.put(26, 26, '%').put(26, 22, '*')
clouds.put(3, 15, 'S')

gusts = room('gusts', 'The Gusts', 'sky', 56, 30)
gusts.settings['wind'] = '-70'
gusts.door('L', 15).door('R', 15)
gusts.fill(0, 7, 0, 14, '#').fill(48, 55, 0, 14, '#')
pit(gusts, 8, 47, 1, 1)
gusts.fill(24, 27, 10, 12, '#').fill(36, 39, 11, 13, '#')
gusts.put(14, 19, 'R', to='6,0', period='4')
gusts.put(31, 20, '%').put(43, 20, '%')
gusts.put(37, 19, '*')
gusts.put(3, 15, 'S')

ledge = room('last_ledge', 'The Last Ledge', 'sky', 36, 26)
ground(ledge, 14)
ledge.door('L', 15).door('R', 15)
ledge.put(10, 15, 'B').put(5, 15, 'N', dialogue='note_sky', look='note')
ledge.fill(20, 27, 18, 19, '#').put(18, 22, '%')
ledge.door('T', 22)
ledge.put(3, 15, 'S')

eye = room('eye_of_storm', 'The Eye of the Storm', 'sky', 36, 26)
eye.settings['wind'] = '110'
ground(eye, 4)
eye.door('B', 22).fill(22, 25, 1, 4, '.').fill(22, 25, 1, 1, '-')
pit(eye, 2, 17, 1, 4)
pit(eye, 28, 34, 1, 4)
for x0, top in [(4, 8), (9, 10), (14, 12)]:
    eye.fill(x0, x0 + 2, top, top, '_')
eye.fill(1, 3, 1, 14, '#')
eye.put(8, 16, 'R', to='6,0', period='4')
eye.put(6, 12, '*').put(2, 15, '*').put(31, 12, '*')
eye.fill(31, 31, 15, 25, '#').put(31, 14, '%')
eye.put(20, 5, 'S')

# ----------------------------------------------------------------------------------------------
# Scenery
# ----------------------------------------------------------------------------------------------

# The Crevasse: dark rock, a crack of daylight far above where you fell, and Ash's cold camp.
a.props((18, 20, 'light_shaft'), (10, 17, 'stalactites'), (28, 17, 'stalactites'),
        (24, 5, 'stalagmite'), (33, 5, 'crystals'))
b.props((8, 16, 'stalactites'), (20, 16, 'stalactites'), (14, 5, 'stalagmite'), (35, 8, 'crystals'))
c.props((14, 15, 'stalactites'), (33, 15, 'stalactites'), (24, 5, 'bones'), (38, 5, 'stalagmite'))
d.props((6, 5, 'cold_fire'), (12, 5, 'pack'), (1, 5, 'bedroll'),
        (18, 18, 'stalactites'), (40, 18, 'stalactites'), (30, 6, 'crystals'), (44, 6, 'stalagmite'))
e.props((36, 16, 'stalactites'), (34, 5, 'stalagmite'), (40, 5, 'crystals'))
f.props((35, 18, 'stalactites'), (6, 5, 'bones'), (30, 5, 'stalagmite'), (44, 5, 'crystals'))

# The Cave: Wren's fire and lantern by the bench. The Well: an old chain, a lamp at the top.
cave.props((10, 5, 'fire'), (4, 15, 'hanging_lantern'), (26, 15, 'stalactites'), (35, 15, 'stalactites'),
           (34, 5, 'stalagmite'), (45, 5, 'crystals'))
well.props((20, 36, 'chains'), (4, 36, 'hanging_lantern'), (8, 5, 'stalagmite'), (22, 10, 'crystals'))

# The Crossroads: Pip's camp, a signpost, flowers, vines from the rim above.
hub.props((23, 5, 'map_table'), (34, 5, 'tent'), (4, 5, 'signpost'), (40, 5, 'lantern_post'),
          (45, 5, 'flowers'), (52, 5, 'flowers'), (2, 30, 'vine'), (57, 30, 'vine'))

# The ruins: broken pillars, statues, banners, glowing mushrooms; the Archive's books and candles.
gate.props((8, 5, 'broken_pillar'), (41, 5, 'statue'), (10, 17, 'banner'), (33, 17, 'banner'), (24, 5, 'mushrooms'))
hollow.props((28, 10, 'mushrooms'), (15, 15, 'mushrooms'), (38, 5, 'broken_pillar'), (30, 32, 'banner'), (6, 25, 'statue'))
archive.props((20, 5, 'bookshelf'), (27, 5, 'bookshelf'), (25, 10, 'candles'), (8, 15, 'candles'), (12, 24, 'candles'),
              (30, 26, 'hanging_lantern'), (2, 26, 'banner'))

# The garden: flowers and vines; Curtains, curtains of vines.
garden.props((4, 5, 'flowers'), (45, 10, 'flowers'), (5, 18, 'vine'), (21, 18, 'vine'), (36, 18, 'vine'),
             (55, 18, 'vine'), (52, 5, 'lantern_post'))
curtains.props((8, 26, 'vine'), (24, 26, 'vine'), (32, 26, 'vine'), (38, 26, 'vine'), (20, 10, 'flowers'), (36, 10, 'flowers'))

# The mountains: ice, pines, Juniper's camp, cairns on the way up.
hall.props((16, 7, 'icicles'), (5, 14, 'icicles'), (16, 21, 'icicles'), (12, 33, 'icicles'), (16, 3, 'cairn'))
summit.props((9, 5, 'tent'), (17, 5, 'fire'), (1, 5, 'pine'), (43, 5, 'pine'), (46, 5, 'flags'), (32, 5, 'cairn'))
stones.props((5, 16, 'pine'), (12, 23, 'icicles'), (32, 23, 'icicles'), (52, 23, 'icicles'), (56, 5, 'cairn'))

# The ember chimney and the pendulums: braziers and chains.
chimney.props((5, 13, 'brazier'), (11, 21, 'brazier'), (5, 29, 'brazier'), (12, 37, 'brazier'))
pendulums.props((26, 24, 'chains'), (35, 24, 'chains'), (44, 24, 'chains'), (16, 5, 'brazier'), (56, 5, 'brazier'))

# Daylight: prayer flags and cairns, and at the top Ash's signal fire.
backtrack.props((8, 5, 'flags'), (34, 15, 'cairn'), (12, 5, 'flowers'), (26, 5, 'signpost'))
windy.props((7, 15, 'cairn'), (24, 15, 'flags'), (45, 15, 'cairn'))
daylight.props((40, 23, 'signal_fire'), (3, 15, 'flags'), (20, 17, 'cairn'))

# ----------------------------------------------------------------------------------------------
# How they join: (room, side, which door of that side) to (room, side, which door).
# ----------------------------------------------------------------------------------------------

JOINS = [
    # The Crevasse, up to The Cave and The Well, and The Deep below the Well
    ('crevasse_wake', 'R', 0, 'crevasse_steps', 'L', 0),
    ('crevasse_steps', 'R', 0, 'crevasse_gaps', 'L', 0),
    ('crevasse_gaps', 'R', 0, 'crevasse_camp', 'L', 0),
    ('crevasse_camp', 'R', 0, 'crevasse_dips', 'L', 0),
    ('crevasse_dips', 'R', 0, 'crevasse_chasm', 'L', 0),
    ('crevasse_chasm', 'R', 0, 'level1', 'L', 0),
    ('level1', 'R', 0, 'level2', 'L', 0),
    ('level2', 'B', 0, 'the_deep', 'T', 0),
    ('level2', 'T', 0, 'crossroads', 'B', 0),
    # West: the ruins
    ('ruins_gate', 'R', 0, 'crossroads', 'L', 0),
    ('lever_works', 'R', 0, 'ruins_gate', 'L', 0),
    ('lever_works', 'T', 0, 'the_vault', 'B', 0),
    ('ruins_hollow', 'R', 0, 'lever_works', 'L', 0),
    ('dark_stair', 'R', 0, 'ruins_hollow', 'L', 0),
    ('ruins_archive', 'R', 0, 'dark_stair', 'L', 0),
    # East: the Market and the garden
    ('crossroads', 'R', 0, 'market', 'L', 0),
    ('market', 'R', 0, 'level3', 'L', 0),
    ('level3', 'R', 0, 'garden_rings', 'L', 0),
    ('garden_rings', 'R', 0, 'greenhouse', 'L', 0),
    ('greenhouse', 'T', 0, 'thorn_walk', 'B', 0),
    ('greenhouse', 'R', 0, 'trellis', 'L', 0),
    ('trellis', 'R', 0, 'level4', 'L', 0),
    # Up: the Tall Hall, the Summit and the Drop back down
    ('crossroads', 'T', 0, 'tall_hall', 'B', 0),
    ('crossroads', 'T', 1, 'the_drop', 'B', 0),
    ('tall_hall', 'T', 0, 'level5', 'B', 0),
    ('the_drop', 'T', 0, 'level5', 'B', 1),
    # The Frost Caves, west of the Summit
    ('icefall', 'R', 0, 'level5', 'L', 0),
    ('frozen_lake', 'R', 0, 'icefall', 'L', 0),
    ('frost_camp', 'R', 0, 'frozen_lake', 'L', 0),
    ('frost_camp', 'T', 0, 'hall_of_icicles', 'B', 0),
    ('icicle_gallery', 'R', 0, 'frost_camp', 'L', 0),
    ('frozen_camp', 'R', 0, 'icicle_gallery', 'L', 0),
    # The Ember Chimney, east of the Summit
    ('level5', 'R', 0, 'level6', 'L', 0),
    ('level6', 'R', 0, 'forge', 'L', 0),
    ('forge', 'R', 0, 'level7', 'L', 0),
    ('level7', 'T', 0, 'level8', 'B', 0),
    ('level8', 'R', 0, 'crumbling_halls', 'L', 0),
    ('crumbling_halls', 'B', 0, 'furnace', 'T', 0),
    ('crumbling_halls', 'R', 0, 'the_updraft', 'L', 0),
    # The Sky
    ('the_updraft', 'R', 0, 'level9', 'L', 0),
    ('level9', 'R', 0, 'windy_ledge', 'L', 0),
    ('windy_ledge', 'R', 0, 'cloud_steps', 'L', 0),
    ('cloud_steps', 'R', 0, 'gusts', 'L', 0),
    ('gusts', 'R', 0, 'last_ledge', 'L', 0),
    ('last_ledge', 'T', 0, 'eye_of_storm', 'B', 0),
    ('last_ledge', 'R', 0, 'level10', 'L', 0),
]
# The project's order: a new game starts in the first room, and the last one has the way out.
# The regions, as the map names them (assets/regions.txt) and docs/world.md has them.
REGIONS = {
    'crevasse': ['crevasse_wake', 'crevasse_steps', 'crevasse_gaps', 'crevasse_camp', 'crevasse_dips', 'crevasse_chasm'],
    'hollows': ['level1', 'level2', 'the_deep'],
    'crossroads': ['crossroads', 'market'],
    'ruins': ['ruins_gate', 'lever_works', 'the_vault', 'ruins_hollow', 'dark_stair', 'ruins_archive'],
    'garden': ['level3', 'garden_rings', 'greenhouse', 'thorn_walk', 'trellis', 'level4'],
    'summit': ['tall_hall', 'level5', 'the_drop'],
    'frost': ['icefall', 'frozen_lake', 'frost_camp', 'hall_of_icicles', 'icicle_gallery', 'frozen_camp'],
    'ember': ['level6', 'forge', 'level7', 'level8', 'crumbling_halls', 'furnace', 'the_updraft'],
    'sky': ['level9', 'windy_ledge', 'cloud_steps', 'gusts', 'last_ledge', 'eye_of_storm', 'level10'],
}
for region, names in REGIONS.items():
    for name in names:
        assert 'region' not in ROOMS[name].settings, (name, 'in two regions')
        ROOMS[name].settings['region'] = region
assert all('region' in r.settings for r in ROOMS.values()), [n for n, r in ROOMS.items() if 'region' not in r.settings]

ORDER = [name for name in ROOMS if name != 'level10'] + ['level10']

pos = {'crevasse_wake': (0, 0)}
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
for i, n in enumerate(names):
    for m in names[i + 1:]:
        (x1, y1), (x2, y2) = pos[n], pos[m]
        A, B = ROOMS[n], ROOMS[m]
        if x1 < x2 + B.w and x2 < x1 + A.w and y1 < y2 + B.h and y2 < y1 + A.h:
            sys.exit(f'{n} and {m} overlap')

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
if len(sys.argv) > 1:
    print(f'wrote {len(ROOMS)} rooms to {OUT}')
else:
    project = os.path.join(ROOT, 'assets', 'world.ldtk')
    subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'rooms.py'), 'import', os.path.join(OUT, 'rooms.txt'), project], check=True)
    print(f'wrote {len(ROOMS)} rooms to {os.path.relpath(project, ROOT)}')
