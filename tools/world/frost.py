"""The Frost Caves, off the old lookout above the campfire: for when you have the long rope (from
the Hook Garden). Ice underfoot, icicles overhead, crystal spikes below; at the top of the Frozen
Falls, the throwing glove."""
from world.build import region, room, ground, roof, pit

region('frost')

# ----------------------------------------------------------------------------------------------
# The Frost Caves: in from the right, a chasm of crystal spikes too wide to jump, and over it a
# hook block hanging on a column of slick ice-polished stone: out of reach of Odo's rope from the
# edge (11 tiles), not of the long rope. Swing across to the far side, which is higher, and on.
# A gem on top of a slick pillar by the way in, too high for one jump (the feather's).
# ----------------------------------------------------------------------------------------------

frost = room('frost_caves', 'The Frost Caves', 'ice', 48, 28)
ground(frost, 5)
pit(frost, 16, 29, bottom=1, top=5)
frost.fill(16, 29, 1, 1, 'x')                   # crystal spikes, not metal ones
frost.fill(1, 15, 6, 8, '#')                    # the far side
frost.fill(22, 23, 16, 26, 'o').fill(22, 23, 15, 15, '%')  # the hook block, on its slick column
frost.fill(39, 43, 11, 11, 'o')                 # a slick shelf off the wall
frost.fill(44, 47, 10, 26, 'o')                 # and the wall, slick
frost.door('R', 6).door('L', 9)
frost.put(44, 6, 'S')
frost.put(42, 6, 'T', route='frost', mark='start')
frost.put(41, 12, '*')

frost.props((38, 6, 'stalagmite'), (34, 6, 'crystals'), (31, 6, 'bones'), (6, 9, 'crystals'), (9, 9, 'stalagmite'),
            (20, 26, 'stalactites'), (30, 26, 'stalactites'), (40, 26, 'icicles'), (5, 26, 'icicles'))

# ----------------------------------------------------------------------------------------------
# The Icicle Hall: a long corridor of ice under icicles that let go as you pass beneath: keep
# going, on a floor that's slow to get going on. Then a crystal pit, a hook block high over its
# middle for the long rope, and the way on beyond.
# ----------------------------------------------------------------------------------------------

hall = room('icicle_hall', 'The Icicle Hall', 'ice', 60, 22)
ground(hall, 3)
roof(hall, 12, 28, 59)                          # a low roof over the corridor
roof(hall, 18, 0, 27)
hall.fill(28, 58, 3, 3, '~')                    # ice underfoot
for x in (32, 36, 40, 44, 48, 52):
    hall.put(x, 11, 'Y')                        # icicles
pit(hall, 8, 25, bottom=1, top=3)
hall.fill(8, 25, 1, 1, 'x')
hall.fill(15, 18, 14, 17, '#').fill(16, 17, 13, 13, '%')  # the hook block over the pit
hall.door('R', 4).door('L', 4)
hall.put(56, 4, 'S')
hall.put(30, 7, '*')                            # among the icicles, a jump up

hall.props((57, 4, 'crystals'), (29, 4, 'stalagmite'), (4, 4, 'crystals'), (2, 4, 'stalagmite'), (12, 17, 'icicles'),
           (22, 17, 'icicles'), (4, 17, 'stalactites'))

# ----------------------------------------------------------------------------------------------
# The Frozen Falls: a waterfall frozen solid, up the far wall of a crystal-floored cave. Its ledges
# are ice, too slippery to climb by hand: hook blocks over them, a long rope's throw up from each to
# the next. At the top, the throwing glove, the frost's beacon, a bench and the finish. Up on the
# left, a shelf under a slick roof, a jump and a half above the top ledge (the feather's).
# ----------------------------------------------------------------------------------------------

falls = room('frozen_falls', 'The Frozen Falls', 'ice', 48, 32)
ground(falls, 3)
roof(falls, 29)
pit(falls, 4, 29, bottom=1, top=3)
falls.fill(4, 29, 1, 1, 'x')
falls.fill(20, 30, 1, 11, '~').fill(20, 30, 1, 10, '#')    # the first ledge, iced on top
falls.fill(31, 38, 15, 18, '#').fill(32, 36, 14, 14, '%')  # rock over it, hook blocks under
falls.fill(8, 15, 1, 19, '#').fill(8, 15, 20, 20, '~')     # the top ledge
falls.fill(16, 21, 26, 28, '#').fill(17, 20, 25, 25, '%')  # rock over the gap, hook blocks under
falls.fill(1, 4, 25, 25, 'o')                   # the shelf up on the left, slick
falls.fill(0, 16, 29, 29, 'o').fill(0, 0, 21, 28, 'o')  # and the roof and the wall round it
falls.door('R', 4)
falls.put(44, 4, 'S')
falls.put(11, 21, 'P', ability='air_throw')
falls.put(13, 21, 'T', route='frost', mark='finish')
falls.put(14, 21, 'B')
falls.put(2, 26, '*')

falls.prop(9, 21, 'beacon', needs='air_throw')
falls.props((42, 4, 'crystals'), (38, 4, 'stalagmite'), (27, 12, 'crystals'), (10, 28, 'icicles'), (28, 28, 'icicles'),
            (40, 28, 'icicles'))
