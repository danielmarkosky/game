"""The opening: where a new game starts. You come to floating in the Hidden Lake, under the waterfall
you fell from, swim ashore and walk out of the cave. Odo, mending the old road outside, gives you a
rope with a hook; the Gully teaches it; and the road comes to Fen's campfire, where the ways go on:
east and down (open from the start), off the lookout on the left and up into the sky (for what's at
the ends of those two), and up the planks to the Old Station (for all four). Each is a route of its
own, in its own file. Behind the lake's waterfall the rock isn't there: through it, the Still, and
up inside it the Hidden Spring; down a crack in its bed, the Drowned Grotto."""
from world.build import region, room, ground, roof

region('lake')

# ----------------------------------------------------------------------------------------------
# The Hidden Lake: a little cave with a lake in it, a waterfall pouring into it from a crack in the
# ceiling on the left. You come to floating in the middle of it.
# ----------------------------------------------------------------------------------------------

lake = room('lake', 'The Hidden Lake', 'grotto', 50, 26)
ground(lake, 1)
roof(lake, 21)
lake.fill(0, 2, 2, 20, '#')                     # the wall behind the waterfall
lake.fill(4, 7, 21, 24, '.')                    # the crack it pours out of
lake.fill(3, 26, 2, 6, 'w')                     # the lake: its surface level with the shore
lake.fill(14, 15, 2, 6, '#')                    # a rock in the middle, to climb out on
lake.fill(19, 20, 2, 2, '#').fill(21, 22, 2, 3, '#').fill(23, 24, 2, 4, '#').fill(25, 26, 2, 5, '#')  # the bed rising
lake.fill(27, 49, 2, 6, '#')                    # the shore
lake.fill(36, 49, 16, 20, '#').fill(46, 49, 11, 15, '#')  # the cave narrowing to the way out
lake.door('R', 7)
# Behind the waterfall, the wall isn't there: '?' is a hidden passage, rock to look at, nothing to
# touch. Swim into the falls and you step out of the water into it. In it, on the left, a way out
# of the world as you know it (the Still, tools/world/still.py); and steps going up inside the
# wall, to where the rock over the crack is in reach of the long rope (12 tiles), not of Odo's (8).
# Hook it, reel in, swing over to the passage at the top of the crack: the Hidden Spring
# (tools/world/secret.py) is along it, on the left. Nothing shows from outside.
lake.fill(0, 4, 7, 10, '?').fill(1, 3, 11, 19, '?')  # behind the falls, and a chimney up inside the wall
lake.door('L', 7).fill(0, 0, 7, 10, '?')
lake.fill(2, 3, 9, 9, '#').fill(1, 2, 12, 12, '#')  # steps in it
lake.fill(3, 3, 20, 20, '#')                    # under the passage
lake.fill(0, 3, 21, 24, '?')                    # the passage at the top of the crack
lake.door('L', 21).fill(0, 0, 21, 24, '?')
# Down in the lake bed, a crack going deeper than anyone can hold their breath: the Drowned Grotto
# (tools/world/drowned.py), for when you can (the diving suit).
lake.fill(8, 9, 0, 1, 'w')
lake.door('B', 8, 2).fill(8, 9, 0, 0, 'w')
lake.put(10, 7, 'S')                            # just above the water: you come to floating
lake.put(31, 7, 'B')                            # a bench on the shore
lake.put(40, 14, '*')                           # up under the overhang: for a rope that reaches

# ----------------------------------------------------------------------------------------------
# The Old Road, outside the cave mouth: Odo, mending the road where the bridge washed out. He gives
# you the rope, and it's the way across.
# ----------------------------------------------------------------------------------------------

region('vale')

road = room('old_road', 'The Old Road', 'glade', 56, 30)
ground(road, 6)
road.fill(0, 14, 15, 28, '#').fill(15, 24, 14, 28, '#')  # the cave mouth, and its lip over the road
road.fill(0, 3, 11, 14, '#')
road.fill(17, 24, 2, 6, '.')                    # where the bridge was: a ditch, too wide to jump
road.fill(17, 18, 2, 3, '#')                    # a step back out of it, on this side
road.fill(17, 17, 6, 6, '-').fill(24, 24, 6, 6, '-')  # what's left of the bridge
road.put(20, 14, '%').put(21, 14, '%')          # hook the lip and swing across
road.door('L', 7).door('R', 7)
road.put(4, 7, 'S')
road.put(12, 7, 'N', dialogue='odo', look='odo', until='long_rope')  # he goes up to the station, later
road.put(20, 9, '*')                            # over the ditch, on the way across

# ----------------------------------------------------------------------------------------------
# The Gully: Odo's practice ground. Swing across a ditch; hook an overhang and reel in, up onto a
# ledge; then a long swing over a pool. Fall in and you swim back under the ledge.
# ----------------------------------------------------------------------------------------------

gully = room('gully', 'The Gully', 'glade', 64, 24)
ground(gully, 6)
roof(gully, 21)
# A ditch, a step back out of it on this side, and a rock hanging over it with hook blocks under.
gully.fill(10, 16, 2, 6, '.').fill(10, 10, 2, 3, '#')
gully.fill(11, 15, 14, 20, '#').fill(12, 14, 13, 13, '%')
# A ledge too high to jump, an overhang above it with hook blocks at its end: hook, reel in, step off.
gully.fill(27, 34, 7, 11, '#')
gully.fill(25, 34, 16, 20, '#').fill(25, 26, 15, 15, '%')
# Water under it: a pool on this side, a tunnel under the ledge, and the big pool beyond.
gully.fill(21, 22, 2, 6, 'w').fill(23, 34, 2, 5, 'w')
# The long swing, over the pool, to the far side as high as the ledge.
gully.fill(35, 45, 2, 6, 'w')
gully.fill(46, 63, 7, 11, '#')
gully.fill(38, 44, 18, 20, '#').fill(39, 43, 17, 17, '%')
gully.door('L', 7).door('R', 12)
gully.put(3, 7, 'S')
gully.put(41, 13, '*')                          # on the long swing

# ----------------------------------------------------------------------------------------------
# The Campfire: Fen, sitting by the fire, a tent behind, a bench, and Mags at her stall up the bank.
# The ways on: east along the road (the Hook Garden: the long rope), down a sinkhole (the Hollows: the
# chalk bag), up the old lookout, a scaffold of planks up the cliff on the left, a jump apart: off it
# on the left (the Frost Caves, for the long rope: the throwing glove), on up through the top into the
# sky (the Sky Road, for the chalk bag: the feather); and up planks on the right to the Old Station
# (the cart up the Old Line, for all four: the end). What each needs is in its own rooms; the camp
# lets you go anywhere.
# ----------------------------------------------------------------------------------------------

camp = room('camp', 'The Campfire', 'glade', 48, 40)
ground(camp, 7)
camp.fill(0, 10, 8, 11, '#').fill(11, 13, 8, 10, '#').fill(14, 16, 8, 9, '#').fill(17, 19, 8, 8, '#')  # down to the clearing
camp.door('L', 12).door('R', 8)
# The sinkhole: planks across it to climb back up by, and to land on coming up.
camp.fill(41, 43, 0, 7, '.').fill(41, 43, 1, 1, '-').fill(41, 43, 4, 4, '-')
camp.door('B', 41, 3)
# The lookout, zig-zagging up the cliff. A rock ledge off it on the left, with the way to the Frost
# Caves; at the top, the way up into the sky.
for x0, x1, y in ((7, 11, 14), (2, 6, 17), (7, 11, 20), (4, 8, 23), (8, 12, 26), (4, 8, 29), (8, 12, 32), (3, 7, 35)):
    camp.fill(x0, x1, y, y, '-')
camp.fill(0, 3, 28, 30, '#')
camp.door('L', 31)
camp.door('T', 2)
# Up the right side of the clearing, planks to the Old Station: the last route, the cart up the
# mountain (tools/world/line.py).
for x0, x1, y in ((34, 38, 10), (40, 44, 13), (34, 38, 16), (40, 44, 19), (34, 38, 22), (40, 44, 25), (34, 38, 28)):
    camp.fill(x0, x1, y, y, '-')
camp.fill(43, 47, 30, 31, '#')
camp.door('R', 32)
camp.put(3, 12, 'S')
camp.put(22, 8, 'B')
camp.put(31, 8, 'N', dialogue='fen', look='fen', pose='sit')
camp.put(13, 11, 'N', dialogue='mags', look='mags')   # Mags, at her stall
camp.put(10, 33, '*')                           # up the lookout
camp.put(42, 5, '*')                            # down the sinkhole

# ----------------------------------------------------------------------------------------------
# Scenery: magic in the air everywhere, glowing things growing, light falling from cracks.
# ----------------------------------------------------------------------------------------------

lake.props((4, 24, 'waterfall'), (11, 20, 'light_shaft'),
           (12, 7, 'lily'), (18, 7, 'lily'), (22, 7, 'lily'), (8, 7, 'lily'),
           (15, 7, 'glowshroom'),
           (28, 7, 'bellflowers'), (33, 7, 'glowshroom'), (37, 7, 'violet_crystals'), (41, 7, 'bellflowers'),
           (44, 7, 'glowshroom'), (35, 7, 'mushrooms'),
           (14, 20, 'glow_roots'), (20, 20, 'stalactites'), (26, 20, 'glow_roots'), (31, 20, 'stalactites'),
           (34, 20, 'glow_roots'), (40, 15, 'glow_roots'), (44, 15, 'stalactites'),
           )
road.props((2, 10, 'violet_crystals'), (8, 14, 'glow_roots'), (17, 13, 'glow_roots'),
           (6, 7, 'wheelbarrow'), (9, 7, 'planks'), (10, 7, 'lantern_post'), (14, 7, 'rubble'), (16, 7, 'road_sign'),
           (27, 7, 'glowshroom'), (31, 7, 'bellflowers'), (36, 7, 'runestone'), (41, 7, 'pine'),
           (45, 7, 'bellflowers'), (49, 7, 'glowshroom'), (52, 7, 'flowers'), (20, 2, 'bellflowers'))
gully.props((5, 7, 'signpost'), (7, 7, 'bellflowers'), (13, 2, 'glowshroom'),
           (13, 12, 'glow_roots'), (19, 7, 'flowers'), (24, 7, 'bellflowers'), (29, 15, 'glow_roots'),
           (32, 12, 'glowshroom'), (40, 16, 'glow_roots'), (50, 12, 'runestone'), (54, 12, 'bellflowers'),
           (58, 12, 'glowshroom'), (61, 12, 'flowers'),
           (8, 20, 'stalactites'), (19, 20, 'glow_roots'), (36, 20, 'stalactites'), (48, 20, 'glow_roots'),
           (56, 20, 'stalactites'))
camp.props((1, 12, 'pine'), (5, 12, 'bellflowers'), (11, 11, 'stall'), (18, 9, 'glowshroom'),
           (28, 8, 'fire'), (32, 8, 'log'), (37, 8, 'tent'), (39, 8, 'pack'), (34, 8, 'bedroll'),
           (25, 8, 'lantern_post'), (45, 8, 'signpost'), (40, 8, 'rubble'), (20, 8, 'bellflowers'),
           (9, 15, 'lantern_post'), (1, 31, 'signpost'), (2, 31, 'pine'), (5, 36, 'flags'))
