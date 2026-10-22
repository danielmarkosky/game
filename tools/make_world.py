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
# A chimney, rock inside and slick outside, up to the glove: leap from wall to wall.
summit.fill(34, 34, 8, 21, 'o').fill(35, 35, 8, 21, '#').fill(39, 39, 8, 21, '#').fill(40, 40, 8, 21, 'o')
summit.fill(34, 40, 22, 33, '#')
summit.put(37, 19, 'P', ability='air_throw')
# The way on: up a slick cliff, by a hook too high to throw to from the ground, but not from the top
# of a jump.
summit.fill(50, 62, 1, 15, 'o')
summit.put(47, 19, '%').put(48, 19, '%')
summit.fill(42, 62, 24, 32, 'o')
summit.door('R', 16)
summit.put(36, 5, '*')

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

backtrack = room('level9', 'Backtrack', 'daylight', 52, 30)
ground(backtrack, 4)
backtrack.door('L', 5)
backtrack.fill(30, 50, 5, 14, '#')
backtrack.fill(30, 30, 5, 14, '#')
backtrack.fill(22, 22, 18, 29, '#').put(22, 17, '%')
backtrack.put(44, 15, 'P', ability='double_jump')
backtrack.door('R', 15)
backtrack.put(22, 12, '*')
backtrack.put(4, 5, 'S')

windy = room('windy_ledge', 'The Windy Ledge', 'daylight', 56, 30)
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
    ('crevasse_wake', 'R', 0, 'crevasse_steps', 'L', 0),
    ('crevasse_steps', 'R', 0, 'crevasse_gaps', 'L', 0),
    ('crevasse_gaps', 'R', 0, 'crevasse_camp', 'L', 0),
    ('crevasse_camp', 'R', 0, 'crevasse_dips', 'L', 0),
    ('crevasse_dips', 'R', 0, 'crevasse_chasm', 'L', 0),
    ('crevasse_chasm', 'R', 0, 'level1', 'L', 0),
    ('level1', 'R', 0, 'level2', 'L', 0),
    ('level2', 'T', 0, 'crossroads', 'B', 0),
    ('ruins_gate', 'R', 0, 'crossroads', 'L', 0),
    ('ruins_hollow', 'R', 0, 'ruins_gate', 'L', 0),
    ('ruins_archive', 'R', 0, 'ruins_hollow', 'L', 0),
    ('crossroads', 'R', 0, 'level3', 'L', 0),
    ('level3', 'R', 0, 'level4', 'L', 0),
    ('crossroads', 'T', 0, 'tall_hall', 'B', 0),
    ('crossroads', 'T', 1, 'the_drop', 'B', 0),
    ('tall_hall', 'T', 0, 'level5', 'B', 0),
    ('the_drop', 'T', 0, 'level5', 'B', 1),
    ('level5', 'R', 0, 'level6', 'L', 0),
    ('level6', 'R', 0, 'level7', 'L', 0),
    ('level7', 'T', 0, 'level8', 'B', 0),
    ('level8', 'R', 0, 'level9', 'L', 0),
    ('level9', 'R', 0, 'windy_ledge', 'L', 0),
    ('windy_ledge', 'R', 0, 'level10', 'L', 0),
]
ORDER = list(ROOMS)  # the project's order: a new game starts in the first room

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
