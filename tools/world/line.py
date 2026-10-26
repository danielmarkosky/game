"""The Old Line: the last route, up the mountain to the Lantern, the way you came down. Its cart
won't move until all four beacons are lit (the four routes off the campfire done). The ride: jumps
over gaps, under crystals, the broken bridge over the falls where you fell (a jump and a second
jump, with the feather); then off the cart at the end of the line and up to the Lantern with
everything you've got. Past the Lantern, the way out of the world: the end of the game."""
from world.build import region, room, ground, roof

region('line')


def track(r, x0, x1, y, under='#'):
    """Rails along row y from x0 to x1, on rock (or whatever `under` is) down to the bottom."""
    if under:
        r.fill(x0, x1, 0, y - 1, under)
    return r.fill(x0, x1, y, y, 't')


# ----------------------------------------------------------------------------------------------
# The Old Station, up the right side of the campfire's clearing: the cart on its rails, the four
# beacons on the shelf behind it (lit as the routes are done), a bench. Odo's here once there's a
# line to mend. Walk into the cart and it rolls off, if it will.
# ----------------------------------------------------------------------------------------------

station = room('station', 'The Old Station', 'mine', 40, 20)
ground(station, 1)
roof(station, 15)
track(station, 5, 39, 2, under=None)
station.fill(1, 4, 2, 2, '#')
station.fill(11, 28, 9, 9, '#')                 # the shelf with the beacons
station.door('L', 3).door('R', 3)
station.put(2, 3, 'B')
station.put(1, 3, 'S')
station.put(19, 10, '*')                        # on the shelf with the beacons: a long rope's reach
station.put(4, 3, 'N', dialogue='odo', look='odo', needs='long_rope')
station.put(6, 3, 'T', route='line', mark='start')
station.put(9, 3, 'K', dir='1', needs='long_rope,wall_climb,air_throw,double_jump')
for x, ability in ((13, 'long_rope'), (17, 'wall_climb'), (21, 'air_throw'), (25, 'double_jump')):
    station.prop(x, 10, 'beacon', needs=ability)
station.props((31, 3, 'mine_frame'), (15, 3, 'lantern_post'), (27, 3, 'rubble'), (2, 14, 'chains'), (37, 14, 'chains'))

# ----------------------------------------------------------------------------------------------
# The Old Line: the cart's ride up the mountain. Over a gap, up a step (or into its face), down
# again under a low roof hung with crystals (don't jump), then straight over the next gap, over
# crumbling track, and up.
# ----------------------------------------------------------------------------------------------

line = room('old_line', 'The Old Line', 'mine', 88, 24)
line.fill(0, 87, 0, 0, '#').fill(1, 86, 1, 1, 'x')        # crystals at the bottom of the gaps
track(line, 0, 16, 2)
track(line, 22, 33, 2)
track(line, 34, 45, 5)
track(line, 46, 55, 2)
line.fill(46, 55, 7, 23, '#').fill(46, 55, 6, 6, 'x')     # the low roof, crystals under it
track(line, 62, 75, 2)
line.fill(66, 69, 2, 2, '&')                    # crumbling track
track(line, 76, 87, 4)
line.door('L', 3).door('R', 5)
line.put(2, 3, 'S')
line.put(28, 7, '*')                            # a jump over the track

line.props((10, 3, 'mine_frame'), (40, 6, 'lantern_post'), (70, 3, 'rubble'), (80, 5, 'mine_frame'))

# ----------------------------------------------------------------------------------------------
# The Broken Bridge: the line crosses the gorge the falls come down, on a trestle bridge. It broke
# under you once; it's still broken: ten tiles of nothing. Jump, and jump again in the air.
# ----------------------------------------------------------------------------------------------

bridge = room('broken_bridge', 'The Broken Bridge', 'mine', 72, 34)
bridge.fill(0, 71, 0, 0, '#').fill(15, 52, 1, 1, 'x')
track(bridge, 0, 14, 4)
track(bridge, 15, 29, 4, under=None)            # the bridge, over the gorge
track(bridge, 40, 52, 4, under=None)
track(bridge, 53, 71, 7)
bridge.door('L', 5).door('R', 8)
bridge.put(2, 5, 'S')
bridge.put(34, 12, '*')                         # over the break

bridge.props((34, 32, 'waterfall'), (6, 5, 'mine_frame'), (60, 8, 'lantern_post'))

# ----------------------------------------------------------------------------------------------
# The Lantern, at the top of the mountain: the end of the line, a buffer the cart stops at. Then up
# to the Lantern itself with all four: climb a wall (the chalk bag), swing over a drop from hook
# blocks too high for Odo's rope (the long rope), throw from a jump to hook blocks 14 tiles up
# (the glove), and jump twice up onto the top (the feather). The Lantern stands there, dark. Past
# it, the way out: the end of the game.
# ----------------------------------------------------------------------------------------------

summit = room('summit', 'The Lantern', 'mine', 48, 44)
summit.fill(0, 47, 0, 0, '#').fill(17, 30, 1, 1, 'x')
track(summit, 0, 12, 7)
summit.fill(13, 16, 0, 16, '#')                 # the wall to climb: higher than two jumps
summit.fill(22, 26, 27, 30, '#').fill(23, 25, 26, 26, '%')  # hook blocks over the drop
summit.fill(31, 36, 0, 17, '#')                 # a ledge beyond it
summit.fill(31, 36, 33, 35, 'o').fill(32, 35, 32, 32, '%')  # hook blocks 14 over it, on slick stone
summit.fill(37, 47, 0, 27, '#').fill(37, 37, 18, 27, 'o')  # the shoulder of the mountain, its face slick
summit.fill(42, 47, 33, 33, 'o')                # the top, slick, a jump and a half over the shoulder
summit.fill(0, 47, 43, 43, 'o').fill(47, 47, 28, 33, 'o').fill(47, 47, 38, 42, 'o')
summit.door('L', 8).door('R', 34)
summit.put(12, 8, 'E')
summit.put(4, 8, 'S')
summit.put(45, 34, 'T', route='line', mark='finish')
summit.put(28, 25, '*')                         # under the first hook blocks

summit.props((43, 34, 'great_lantern_off'), (8, 8, 'mine_frame'), (34, 18, 'cairn'), (40, 28, 'flags'))
