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
# bag, from the den.) By the foot of the shaft, a trapdoor of planks: down to the Quarry
# (tools/world/quarry.py).
# ----------------------------------------------------------------------------------------------

# ----------------------------------------------------------------------------------------------
# The Sinkhole: under the boards in the camp's clearing, a shaft down through the hill. Grass and
# roots hanging in at the top, the rock wetter and darker going down. Boards across it on alternate
# sides, a jump apart, to climb back up by; down the middle, clear, to boards over the drop to the
# Hollows (drop through them).
# ----------------------------------------------------------------------------------------------

sink = room('sinkhole', 'The Sinkhole', 'cave', 12, 24)
for y in (2, 8, 14, 20):
    sink.fill(1, 3, y, y, '-')                  # boards on the left, the top ones under the way up...
for y in (5, 11, 17):
    sink.fill(7, 10, y, y, '-')                 # ...and on the right
sink.fill(4, 6, 1, 1, '-')                      # and boards over the way down, to land on coming up
sink.door('T', 1, 3).door('B', 4, 3)
sink.put(2, 21, 'S')

sink.props((5, 22, 'vine'), (9, 22, 'glow_roots'), (7, 22, 'glow_roots'), (2, 15, 'flowers'), (9, 18, 'mushrooms'),
           (8, 12, 'mushrooms'), (2, 9, 'bones'), (9, 6, 'mushrooms'))

# ----------------------------------------------------------------------------------------------
hollows = room('hollows', 'The Hollows', 'cave', 40, 30)
ground(hollows, 3)
roof(hollows, 10, 0, 14)                        # the low roof on the left
hollows.fill(7, 9, 10, 10, '%')                 # hook blocks in it, over the pit
pit(hollows, 4, 12, bottom=1, top=3)
hollows.fill(21, 38, 4, 26, '#')                # the wall beside the shaft, up to under the hole
hollows.fill(21, 22, 12, 14, '.').fill(21, 22, 20, 22, '.')  # notches in it, to rest in
hollows.door('T', 18, 3).door('L', 4)
hollows.fill(13, 15, 0, 3, '.').door('B', 13, 3).fill(13, 15, 0, 0, '-')  # a trapdoor of planks, down to the Quarry
hollows.put(17, 4, 'S')
hollows.put(19, 4, 'T', route='hollows', mark='start')

hollows.props((16, 4, 'bones'), (3, 9, 'stalactites'), (11, 9, 'stalactites'),
              (24, 27, 'cold_fire'), (30, 27, 'stalagmite'), (35, 27, 'crystals'), (16, 28, 'stalactites'))

# ----------------------------------------------------------------------------------------------
# The Old Workings: where the stone for the camp was cut, long ago. In along the floor on the
# right; the way on is shut by a gate, and a weight plate on the floor would hold it open, but
# you're too light. Up on the shelf over it (hook the rock hanging over its end, reel in, step off),
# a block of cut stone. Hooked from up there, reeling it in only brings it to your feet; hooked,
# then lowering yourself off the end of the shelf and reeling it in from the floor, the rope over
# the edge pulls it along the shelf and off, down onto the plate. Through the gate (a lever beside
# it keeps it open after), a pit of spikes, hook blocks over the middle, and either side more, with
# braziers hanging from them: a rope hooked in those hangs through the fire and burns, and swinging
# too high on the middle ones swings it into a brazier. A gem over the far end of the shelf, too
# high to jump to from it: from on top of the stone, it isn't (or the long rope, on the hook blocks
# over it).
# ----------------------------------------------------------------------------------------------

works = room('old_workings', 'The Old Workings', 'cave', 44, 30)
ground(works, 3)
roof(works, 26)
works.fill(0, 18, 11, 29, '#')                  # the low roof over the pit
works.fill(9, 10, 10, 10, '%')                  # hook blocks in it, over the middle
works.fill(6, 6, 10, 10, '%').fill(13, 13, 10, 10, '%')  # and either side, braziers hanging from them
pit(works, 4, 15, bottom=1, top=3)
works.put(6, 8, 'F').put(13, 8, 'F')
works.fill(19, 21, 10, 25, '#')                 # the wall over the gate
works.fill(20, 20, 4, 9, '=')                   # the gate
works.put(18, 4, 'L')                           # its lever, on the far side
works.put(26, 4, 'V')                           # the weight plate
works.fill(29, 42, 7, 8, '#')                   # the shelf
works.put(38, 9, 'O')                           # the stone on it
works.fill(26, 31, 13, 25, '#').fill(26, 27, 12, 12, '%')  # rock hanging over its end, hook blocks under that
works.fill(36, 39, 21, 25, '#').fill(36, 39, 20, 20, '%')  # and over the far end, for the long rope
works.door('R', 4).door('L', 4)
works.put(40, 4, 'S')
works.put(38, 16, '*')                          # under the far end's hook blocks

works.props((24, 4, 'rubble'), (33, 4, 'wheelbarrow'), (37, 4, 'mine_frame'), (42, 9, 'planks'), (31, 9, 'rubble'),
            (2, 4, 'lantern_post'), (17, 4, 'bones'), (23, 25, 'hanging_lantern'), (6, 9, 'chains'), (13, 9, 'chains'))

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
