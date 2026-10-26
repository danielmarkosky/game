"""The Drowned Grotto, down the crack in the bed of the Hidden Lake: a tunnel under water far
longer than a breath lasts, to a pocket of air with gems in it. For when you can stay under
longer: the diving suit, at Mags's stall."""
from world.build import region, room

region('drowned')

# ----------------------------------------------------------------------------------------------
# The Drowned Grotto: down from the lake bed, a flooded tunnel winding down, along and up again,
# 60 tiles of it and not a breath of air (8 seconds of breath is 45 tiles, swimming; the diving
# suit's 32 is plenty there and back). At its end, a pocket of air, a dry ledge, gems.
# ----------------------------------------------------------------------------------------------

drowned = room('drowned', 'The Drowned Grotto', 'grotto', 50, 26)
drowned.fill(0, 49, 0, 25, '#')
drowned.door('T', 3, 2)
# The tunnel, two tiles wide: down from the crack, along the bottom, up at the far end.
drowned.fill(3, 4, 3, 25, 'w')
drowned.fill(3, 30, 3, 4, 'w')
drowned.fill(29, 30, 3, 14, 'w')
drowned.fill(9, 30, 13, 14, 'w')
drowned.fill(9, 10, 13, 20, 'w')
drowned.fill(9, 34, 19, 20, 'w')
drowned.fill(33, 34, 6, 20, 'w')
# The pocket of air at the end: its water level with a ledge to climb out onto, air over both.
drowned.fill(35, 41, 6, 10, 'w').fill(35, 47, 11, 16, '.').fill(35, 35, 11, 16, '#')
drowned.fill(42, 47, 6, 10, '#')
drowned.put(44, 11, '*').put(46, 11, '*').put(38, 15, '*')
drowned.put(42, 11, 'S')

drowned.props((43, 11, 'glowshroom'), (47, 11, 'violet_crystals'), (40, 16, 'glow_roots'))
