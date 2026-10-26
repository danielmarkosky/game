"""The Hollows, under the campfire: the second of the two routes open from the start. Down the
sinkhole, over crumbling stone, through a hollow so dark you have to light your own way, to the
den at the bottom: the chalk bag, and Pip, waiting up on a ledge. The chalk bag is what the Sky Road
needs. Back up: climbing, or from the bench down there."""
from world.build import region, room, ground, roof, pit

region('hollows')

# ----------------------------------------------------------------------------------------------
# The Hollows: a drop down a shaft to the floor of a cave. Left, under a low roof, a pit of spikes
# with hook blocks over it, and the way on beyond it. Back up, the wall beside the shaft: climb
# it, resting in the notches cut in it, onto the top, and jump up through the hole. (With the chalk
# bag, from the den.)
# ----------------------------------------------------------------------------------------------

hollows = room('hollows', 'The Hollows', 'cave', 40, 30)
ground(hollows, 3)
roof(hollows, 10, 0, 14)                        # the low roof on the left
hollows.fill(7, 9, 10, 10, '%')                 # hook blocks in it, over the pit
pit(hollows, 4, 12, bottom=1, top=3)
hollows.fill(21, 38, 4, 26, '#')                # the wall beside the shaft, up to under the hole
hollows.fill(21, 22, 12, 14, '.').fill(21, 22, 20, 22, '.')  # notches in it, to rest in
hollows.door('T', 18, 3).door('L', 4)
hollows.put(17, 4, 'S')
hollows.put(19, 4, 'T', route='hollows', mark='start')

hollows.props((15, 4, 'bones'), (13, 4, 'mushrooms'), (3, 9, 'stalactites'), (11, 9, 'stalactites'),
              (24, 27, 'cold_fire'), (30, 27, 'stalagmite'), (35, 27, 'crystals'), (16, 28, 'stalactites'))

# ----------------------------------------------------------------------------------------------
# The Dark Hollow: too dark to see more than a step ahead, but for lamps hanging in it; a rope
# thrown at one lights it, for good. Under them, a pit of spikes, and over it stone that crumbles a
# moment after you stand on it: keep going. In the middle a rock to rest on; more crumbling stone
# beyond it, and hook blocks over that for anyone who'd rather swing. A gem up high on the right,
# on a ledge only a long rope gets to.
# ----------------------------------------------------------------------------------------------

dark = room('dark_hollow', 'The Dark Hollow', 'cave', 48, 24)
dark.settings['dark'] = 1
ground(dark, 3)
roof(dark, 16)
pit(dark, 7, 39, bottom=1, top=3)
dark.fill(31, 38, 3, 3, '&').fill(26, 28, 3, 3, '&')        # crumbling stone over the spikes
dark.fill(15, 19, 3, 3, '&').fill(9, 12, 3, 3, '&')         # and beyond the rock
dark.fill(21, 23, 1, 5, '#')                    # a rock to rest on
dark.fill(11, 15, 12, 15, '#').fill(12, 14, 11, 11, '%')    # rock hanging over the rest, hook blocks under it
dark.fill(41, 46, 13, 13, '#')                  # a ledge up on the right
dark.door('R', 4).door('L', 4)
dark.put(44, 4, 'S')
for x, y in ((42, 10), (35, 10), (27, 10), (22, 12), (12, 12), (5, 10)):
    dark.put(x, y, 'Q')
dark.put(44, 14, '*')

dark.props((46, 4, 'bones'), (2, 4, 'stalagmite'), (22, 6, 'crystals'), (30, 15, 'stalactites'), (8, 15, 'stalactites'))

# ----------------------------------------------------------------------------------------------
# Pip's Den, at the bottom of it all: in high on the right, and down to the floor of a cave. The
# chalk bag lies there, by a bench, with the hollows' beacon and the finish of the timed run. Up on
# a ledge in the middle, too high to jump to, a dog has been waiting: with the chalk bag, climb the
# rock up to him (resting in the notch halfway). Back out: climb the wall you came down, or sit on
# the bench and go. A gem through the rock at the far left, which isn't there; and one up under
# the roof, for a throw from a jump (the glove).
# ----------------------------------------------------------------------------------------------

den = room('pips_den', "Pip's Den", 'cave', 44, 32)
ground(den, 3)
roof(den, 28)
den.fill(36, 43, 4, 21, '#')                    # the ledge you come in on, its wall to climb
den.fill(36, 36, 9, 10, '.').fill(36, 36, 16, 17, '.')  # with notches in it
den.fill(18, 23, 7, 16, '#')                    # the rock with Pip on top, a way under it
den.fill(23, 23, 10, 11, '.')                   # a notch in it
den.fill(1, 2, 4, 6, '?')                       # rock that isn't there
den.fill(1, 42, 28, 28, 'o')                    # the roof slick: nothing to hook but
den.fill(10, 12, 27, 27, '%')                   # hook blocks up under it
den.door('R', 22)
den.put(40, 22, 'S')
den.put(8, 4, 'P', ability='wall_climb')
den.put(10, 4, 'T', route='hollows', mark='finish')
den.put(4, 4, 'B')
den.put(20, 17, 'N', dialogue='pip', look='pip', until='dog')
den.put(1, 4, '*')
den.put(11, 22, '*')

den.prop(13, 4, 'beacon', needs='wall_climb')
den.props((15, 4, 'bones'), (26, 4, 'mushrooms'), (31, 4, 'crystals'), (33, 4, 'stalagmite'), (39, 22, 'crystals'),
          (6, 27, 'stalactites'), (16, 27, 'stalactites'), (28, 27, 'stalactites'), (21, 17, 'bones'))
