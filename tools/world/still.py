"""The Still: behind the Hidden Lake's waterfall, through the wall that isn't there, out of the world
and into the place between the stars the falls pour from. Nothing falls here: you float, pushed
along with the arrows any way you like. At the far end, Gran's stopwatch, which has been waiting
for you a long time."""
from world.build import region, room

region('still')

# ----------------------------------------------------------------------------------------------
# The Still: in from the right, onto a rock in the dark, and off it into nothing. Rocks adrift,
# crystals growing on some (they cut, as anywhere), stars come down to rest on others. Down and
# left, past a rock with crystals all over its face, the way on. Nothing's needed: only the arrows.
# ----------------------------------------------------------------------------------------------

still = room('still', 'The Still', 'still', 48, 24)
still.settings['space'] = 1
still.door('R', 13).door('L', 3)
still.fill(43, 47, 9, 12, '#')                  # the rock you come in on
still.fill(34, 38, 4, 7, '#').fill(34, 38, 3, 3, 'x')   # rocks adrift: crystals under this one
still.fill(24, 27, 14, 22, '#').fill(23, 23, 15, 22, 'x')  # one hanging down, crystals on its face
still.fill(24, 27, 1, 9, '#').fill(23, 23, 1, 8, 'x')      # and one rising to meet it: between them, the way
still.fill(12, 17, 10, 12, '#').fill(12, 17, 13, 13, 'x')  # one in the middle, crystals on top
still.fill(3, 7, 9, 22, '#')                    # a big one on the left, with a gem tucked under it
still.fill(3, 7, 8, 8, 'x').fill(5, 5, 8, 8, '#')
still.fill(8, 9, 14, 17, '?')                   # and a pocket in it that isn't there
still.fill(4, 7, 14, 17, '?')
still.put(41, 13, 'T', route='still', mark='start')
still.put(30, 18, '*')                          # up between the rocks
still.put(5, 15, '*')                           # in the pocket
still.put(36, 1, '*')                           # under the crystals
still.put(45, 13, 'S')

# ----------------------------------------------------------------------------------------------
# Where the Falls Begin: a winding way between crystals, wider and narrower, the light of the
# stars come down to rest along it; at its end, the rift the waterfall pours out of, and the
# stopwatch lying in front of it.
# ----------------------------------------------------------------------------------------------

source = room('falls_source', 'Where the Falls Begin', 'still', 44, 26)
source.settings['space'] = 1
source.door('R', 3)
source.fill(33, 37, 9, 12, '#').fill(32, 32, 9, 12, 'x')     # a rock adrift by the way in
source.fill(19, 24, 1, 14, '#').fill(19, 24, 15, 15, 'x')    # a wall up the middle, crystals along its top
source.fill(12, 31, 22, 24, '#').fill(13, 30, 21, 21, 'x')   # and the roof hanging low over it, crystals under it
source.fill(8, 10, 1, 19, '#').fill(11, 11, 2, 19, 'x')      # a pillar, crystals down its near side: over its top
source.fill(1, 5, 1, 3, '#')                                 # the rift's ledge
source.put(3, 4, 'P', ability='stopwatch')
source.put(5, 4, 'T', route='still', mark='finish')
source.put(9, 21, '*')                          # over the pillar
source.put(27, 18, '*')                         # between the wall and the roof
source.put(40, 3, 'S')

still.props((46, 13, 'star_cluster'), (36, 8, 'star_cluster'), (14, 14, 'star_cluster'), (30, 1, 'star_cluster'))
source.props((2, 4, 'rift'), (27, 1, 'star_cluster'), (35, 13, 'star_cluster'), (15, 1, 'star_cluster'))
