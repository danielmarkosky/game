"""The Hook Garden, east of the campfire along the road: the first of the two routes open from the
start. Swinging over spikes, then bounce mushrooms, then hook rings that carry you, and at its end
the long rope, on the hill where the garden's beacon stands. The long rope is what the Frost Caves
need."""
from world.build import region, room, ground, roof, pit

region('garden')

# ----------------------------------------------------------------------------------------------
# The Hook Garden: a pit of spikes too wide to jump, an island in the middle of it, and over each
# half a rock hanging from the roof with hook blocks under it: two swings. Over the far side, a
# ledge in the air with a gem on it: Odo's rope only hooks its underside; the long rope reaches the
# roof over it (a secret for later).
# ----------------------------------------------------------------------------------------------

garden = room('hook_garden', 'The Hook Garden', 'garden', 56, 24)
ground(garden, 7)
roof(garden, 19)
pit(garden, 12, 40, bottom=1, top=7)
garden.fill(24, 28, 1, 7, '#')                  # the island
garden.fill(16, 19, 15, 18, '#').fill(16, 19, 14, 14, '%')
garden.fill(32, 35, 15, 18, '#').fill(32, 35, 14, 14, '%')
garden.fill(46, 49, 15, 15, '#')                # the ledge in the air
garden.door('L', 8).door('R', 8)
garden.put(3, 8, 'S')
garden.put(6, 8, 'T', route='garden', mark='start')
garden.put(47, 16, '*')

garden.props((5, 8, 'flowers'), (8, 8, 'bellflowers'), (10, 8, 'signpost'), (27, 8, 'flowers'),
             (46, 8, 'bellflowers'), (49, 8, 'flowers'), (6, 18, 'vine'), (24, 18, 'vine'),
             (30, 18, 'vine'), (42, 18, 'vine'), (48, 18, 'vine'))

# ----------------------------------------------------------------------------------------------
# The Mushroom Glade: bounce mushrooms. One on the ground first, to bounce up onto a ledge with;
# then mushrooms on pillars over a pit of spikes, one bounce to the next, and onto the far side.
# A gem over the middle of them; another in a hollow in the roof, out of reach of a bounce (the
# feather's second jump gets there, or a rope long enough to hook the roof).
# ----------------------------------------------------------------------------------------------

glade = room('mushroom_glade', 'The Mushroom Glade', 'garden', 56, 26)
ground(glade, 7)
roof(glade, 22)
glade.fill(9, 11, 8, 8, 'm')                    # a mushroom on the ground
glade.fill(13, 19, 12, 13, '#')                 # a ledge up from it
pit(glade, 21, 42, bottom=1, top=7)
for x in (24, 31, 38):                          # mushrooms on pillars over the spikes
    glade.fill(x, x + 1, 2, 10, '#').fill(x, x + 1, 11, 11, 'm')
glade.fill(43, 55, 8, 13, '#')                  # the far side
glade.fill(27, 29, 22, 23, '.')                 # a hollow in the roof
glade.door('L', 8).door('R', 14)
glade.put(3, 8, 'S')
glade.put(35, 17, '*')                          # over the mushrooms
glade.put(28, 23, '*')                          # in the hollow

glade.props((4, 8, 'mushrooms'), (6, 8, 'flowers'), (15, 14, 'mushrooms'), (18, 14, 'flowers'), (46, 14, 'mushrooms'),
            (50, 14, 'flowers'), (53, 14, 'bellflowers'), (8, 21, 'vine'), (20, 21, 'vine'), (34, 21, 'vine'),
            (46, 21, 'vine'))

# ----------------------------------------------------------------------------------------------
# The Ring Orchard: hook rings sliding along tracks over a last pit of spikes. Catch the first,
# let it carry you, let go over a mushroom; bounce up onto a ledge in the middle; catch the
# second from there, and over to the hill at the end, with the long rope on it, the garden's
# beacon (it lights once you've the long rope), a bench, and the finish of the timed run. A gem
# in the rock at the end of the hill: the rock there isn't there.
# ----------------------------------------------------------------------------------------------

orchard = room('ring_orchard', 'The Ring Orchard', 'garden', 60, 28)
ground(orchard, 7)
roof(orchard, 24)
orchard.fill(0, 9, 8, 13, '#')                  # the ledge you come in on
pit(orchard, 10, 49, bottom=1, top=7)
orchard.fill(25, 27, 2, 8, '#').fill(25, 27, 9, 9, 'm')   # a mushroom island
orchard.fill(31, 35, 2, 15, '#')                # the ledge in the middle
orchard.fill(50, 59, 8, 13, '#')                # the hill at the end
orchard.put(9, 19, 'R', to='12,0', period='4')
orchard.put(36, 21, 'R', to='12,0', period='4')
orchard.fill(57, 58, 14, 17, '?')               # rock that isn't there, a gem behind it
orchard.door('L', 14)
orchard.put(3, 14, 'S')
orchard.put(52, 14, 'P', ability='long_rope')
orchard.put(54, 14, 'T', route='garden', mark='finish')
orchard.put(50, 14, 'B')
orchard.put(58, 15, '*')
orchard.put(33, 19, '*')                        # over the middle ledge

orchard.prop(56, 14, 'beacon', needs='long_rope')
orchard.props((2, 14, 'flowers'), (6, 14, 'bellflowers'), (33, 16, 'flowers'),
              (10, 23, 'vine'), (24, 23, 'vine'), (40, 23, 'vine'), (50, 23, 'vine'))
