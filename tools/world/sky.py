"""Up from the camp: the Cliff Path, the way to everywhere above the clearing; and from its top, the
Sky Road: for when you have the chalk bag (from the Hollows). A wall to climb, clouds that won't hold
you for long, updrafts in the wind, and at the Sky's Edge, the updraft feather."""
from world.build import region, room

region('vale')

# ----------------------------------------------------------------------------------------------
# The Cliff Path: up through the boards over the camp's last shelf, onto the cliff above it, where
# the trees thin out and the sky opens up. Shelves of rock up both sides of it, a jump apart, from
# one to the other. Off one on the left, the path into the snow (the Snowy Pass, the Frost Caves);
# off one on the right, the planks to the Old Station; at the top, out under the open sky (the Sky
# Road). A gem up under the top, out over the drop.
# ----------------------------------------------------------------------------------------------

cliff = room('cliff_path', 'The Cliff Path', 'glade', 22, 33)
cliff.fill(0, 21, 0, 1, '#')
cliff.fill(18, 20, 0, 1, '.').fill(18, 20, 1, 1, '-')  # boards over the way up from the camp
cliff.door('B', 18, 3)
for top in (5, 11, 17, 23, 29):
    cliff.fill(1, 13, top - 1, top - 1, '#')    # shelves on the left
for top in (8, 14, 20, 26):
    cliff.fill(9, 20, top - 1, top - 1, '#')    # and on the right
cliff.door('L', 17).door('R', 8).door('T', 2)
cliff.put(15, 2, 'S')
cliff.put(19, 30, '*')

cliff.props((3, 2, 'pine'), (8, 2, 'bellflowers'), (12, 2, 'flowers'), (3, 5, 'flowers'), (7, 5, 'pine'),
            (18, 8, 'signpost'), (12, 8, 'bellflowers'), (2, 11, 'pine'), (9, 11, 'flowers'), (16, 14, 'cairn'),
            (3, 17, 'signpost'), (10, 17, 'flowers'), (19, 20, 'pine'), (5, 23, 'cairn'), (14, 26, 'flags'),
            (9, 29, 'flags'), (11, 31, 'hanging_lantern'))

region('sky')

# ----------------------------------------------------------------------------------------------
# The Sky Road: up a short shaft onto a ledge under the open sky, the wall behind it slick. Ahead, a
# wall of rock 16 tiles high: climb it, resting in the notches (out of reach of any rope from
# below). From its top, clouds over a long drop to crystals: they fade a moment after you land on
# one, so keep going, to the ledge on the far side.
# ----------------------------------------------------------------------------------------------

sky = room('sky_road', 'The Sky Road', 'sky', 48, 36)
sky.fill(0, 10, 0, 8, '#')                      # the ledge you come up onto
sky.fill(2, 5, 0, 8, '.').fill(2, 5, 1, 1, '-').fill(2, 5, 4, 4, '-').fill(2, 5, 7, 7, '-')  # the shaft up through it, planks in it
sky.door('B', 2)
sky.fill(0, 0, 9, 34, 'o')                      # the wall behind it, slick
sky.fill(11, 14, 0, 24, '#')                    # the wall to climb
sky.fill(11, 11, 15, 16, '.').fill(11, 11, 20, 21, '.')  # notches in it
sky.fill(15, 46, 1, 1, 'x')                     # crystal spikes far below
for x0 in (18, 25, 32, 39):
    sky.fill(x0, x0 + 3, 24, 24, '_')           # clouds
sky.fill(44, 47, 0, 24, '#')                    # the far side
sky.door('R', 25)
sky.put(8, 9, 'S')
sky.put(6, 9, 'T', route='sky', mark='start')
sky.put(12, 25, '*')                            # on top of the wall

sky.props((4, 9, 'cairn'), (9, 9, 'flags'), (13, 25, 'flags'), (45, 25, 'cairn'))

# ----------------------------------------------------------------------------------------------
# The Updraft Cliffs: a deep shaft in the cliffs, crystals at its bottom, the wind blowing through
# it from the right. Updrafts rise from vents: step into one and it carries you up, as long as you
# hold against the wind. Up the first onto a ledge, along it to the second, and that one carries
# you out of the top. A gem on a ledge off the side of the first.
# ----------------------------------------------------------------------------------------------

cliffs = room('updraft_cliffs', 'The Updraft Cliffs', 'sky', 40, 44)
cliffs.settings['wind'] = -40
cliffs.fill(0, 8, 0, 3, '#')                    # where you come in
cliffs.fill(9, 39, 0, 0, '#').fill(9, 38, 1, 1, 'x')
cliffs.fill(9, 12, 2, 3, '#')                   # the first vent's rock
cliffs.put(10, 4, 'U')
cliffs.fill(8, 13, 33, 34, '#')                 # rock over it: as high as it goes
cliffs.fill(16, 25, 17, 18, '#')                # a ledge out from it
cliffs.put(22, 19, 'U')
cliffs.fill(2, 5, 24, 24, '#')                  # a ledge off the side, with a gem
cliffs.fill(0, 0, 4, 43, 'o').fill(39, 39, 4, 43, 'o')    # slick walls round the shaft
cliffs.door('L', 4).door('T', 21)
cliffs.put(3, 4, 'S')
cliffs.put(3, 25, '*')

cliffs.props((5, 4, 'cairn'), (18, 19, 'flags'))

# ----------------------------------------------------------------------------------------------
# The Sky's Edge: up out of the cliffs, onto planks; clouds stepping up to a peak, the feather on
# it with the sky's beacon, a bench and the finish. Hook blocks straight over the peak, 14 tiles
# up: just out of the long rope's reach standing, not from the top of a jump with the glove; a gem
# up there by them.
# ----------------------------------------------------------------------------------------------

edge = room('sky_edge', "The Sky's Edge", 'sky', 48, 32)
edge.fill(0, 47, 0, 0, '#').fill(1, 18, 1, 1, 'x').fill(27, 46, 1, 1, 'x')
edge.fill(19, 26, 1, 1, '-')                    # planks over the way up
edge.door('B', 21)
for x0, y in ((28, 4), (33, 7), (27, 10)):
    edge.fill(x0, x0 + 3, y, y, '_')            # clouds stepping up
edge.fill(33, 46, 11, 12, '#')                  # the peak
edge.fill(0, 47, 31, 31, 'o')                   # the sky's roof, slick
edge.fill(38, 41, 28, 30, 'o').fill(38, 41, 27, 27, '%')  # hook blocks hanging from it, 14 over the peak
edge.put(22, 2, 'S')
edge.put(43, 13, 'P', ability='double_jump')
edge.put(41, 13, 'T', route='sky', mark='finish')
edge.put(35, 13, 'B')
edge.put(39, 25, '*')

edge.prop(45, 13, 'beacon', needs='double_jump')
edge.props((20, 2, 'cairn'), (38, 13, 'flags'))
