"""The Still: behind the Hidden Lake's waterfall, through the wall that isn't there, out of the world
and into the place between the stars the falls pour from. Nothing falls here, and nothing you do
moves you: you drift on the way you were going, and touching rock you can only cling to it and
clamber along it. The rope is the only way anywhere: hook a rock and reel yourself in to it, or
hold on as you drift past and it swings you round, and let go heading the right way. The walls are
grown over with crystals, so drifting is no way to travel. At the far end, Gran's stopwatch, which
has been waiting for you a long time."""
from world.build import region, room

region('still')


def crystal_walls(r, keep=()):
    """Crystals all along the inside of the walls, but for the (x0, x1, y0, y1) spans in `keep`."""
    r.fill(1, r.w - 2, 1, 1, 'x').fill(1, r.w - 2, r.h - 2, r.h - 2, 'x')
    r.fill(1, 1, 1, r.h - 2, 'x').fill(r.w - 2, r.w - 2, 1, r.h - 2, 'x')
    for x0, x1, y0, y1 in keep:
        r.fill(x0, x1, y0, y1, '.')
    return r


# ----------------------------------------------------------------------------------------------
# The Still: in from the right, onto a rock in the dark. Rocks adrift, too far apart to clamber
# between: hook the next, reel in, let go. Up and left to a rock with stars come down to rest on it,
# then down past a column of crystals hanging in the middle (swing round under it on the rope
# hooked in its rock: straight across, you'd hit them), to a rock low on the left, and the way on
# beside it. Gems: in a pocket in the big rock that isn't there; out on a rock of its own up in the
# corner, a long throw; and under the crystal column, for a close swing.
# ----------------------------------------------------------------------------------------------

still = room('still', 'The Still', 'still', 48, 24)
still.settings['space'] = 1
still.door('R', 13).door('L', 3)
crystal_walls(still, keep=[(46, 46, 9, 17), (1, 7, 1, 7)])
still.fill(41, 46, 9, 12, '#')                  # the rock you come in on
still.fill(30, 34, 16, 18, '#')                 # up and left: stars on this one
still.fill(30, 34, 15, 15, 'x').fill(31, 33, 15, 15, '#')  # crystals at its corners underneath
still.fill(22, 25, 13, 21, '#').fill(21, 21, 14, 21, 'x').fill(26, 26, 14, 21, 'x')  # the column, crystals down both sides
still.fill(22, 25, 12, 12, '%')                 # hook blocks under it, to swing round on
still.fill(34, 37, 4, 6, '#')                   # a rock low in the middle
still.fill(13, 18, 8, 11, '#')                  # the big rock on the left
still.fill(13, 18, 12, 12, 'x')                 # crystals all over its top
still.fill(14, 18, 9, 10, '?')                  # and a pocket in it that isn't there, open on the right
still.fill(2, 7, 1, 2, '#')                     # the rock by the way out
still.fill(40, 42, 19, 20, '#')                 # a rock of its own, up in the corner
still.put(43, 13, 'T', route='still', mark='start')
still.put(15, 9, '*')                           # in the pocket
still.put(41, 21, '*')                          # on the rock in the corner
still.put(24, 9, '*')                           # under the column
still.put(45, 13, 'S')

# ----------------------------------------------------------------------------------------------
# Where the Falls Begin: a winding way between crystals, the light of the stars come down to rest
# along it. In on the right, onto a rock; a rock adrift beyond it, crystals on its top. Then a wall
# up the middle, crystals along its top and the roof hanging low over that, crystals under it: the
# way is under the wall, hook blocks along its underside. Then a low rock, a pillar with crystals
# down its near side, and beyond it the rift the waterfall pours out of, the stopwatch lying in
# front of it. Gems: on top of the pillar, and up between the wall and the roof.
# ----------------------------------------------------------------------------------------------

source = room('falls_source', 'Where the Falls Begin', 'still', 44, 26)
source.settings['space'] = 1
source.door('R', 3)
crystal_walls(source, keep=[(37, 42, 1, 7), (1, 7, 1, 4)])
source.fill(37, 42, 1, 2, '#')                  # the rock you come in on
source.fill(30, 32, 4, 6, '#').fill(30, 32, 7, 7, 'x')         # a rock adrift, crystals on its top
source.fill(19, 24, 7, 14, '#').fill(19, 24, 15, 15, 'x')      # the wall up the middle, crystals along its top
source.fill(19, 24, 6, 6, '%')                  # hook blocks along its underside
source.fill(12, 31, 22, 23, '#').fill(13, 30, 21, 21, 'x')     # the roof hanging low over it, crystals under it
source.fill(8, 10, 9, 19, '#').fill(11, 11, 9, 19, 'x')        # the pillar, crystals down its near side
source.fill(8, 10, 20, 20, '%')                 # hook blocks on top
source.fill(13, 15, 2, 3, '#').fill(13, 15, 4, 4, 'x')         # a low rock, crystals on top
source.fill(2, 6, 1, 3, '#')                    # the rift's ledge
source.put(3, 4, 'P', ability='stopwatch')
source.put(5, 4, 'T', route='still', mark='finish')
source.put(9, 22, '*')                          # on top of the pillar
source.put(27, 18, '*')                         # between the wall and the roof
source.put(40, 3, 'S')

still.props((44, 13, 'star_cluster'), (31, 19, 'star_cluster'), (36, 7, 'star_cluster'), (4, 3, 'star_cluster'))
source.props((2, 4, 'rift'), (31, 8, 'star_cluster'), (14, 5, 'star_cluster'), (39, 3, 'star_cluster'))
