"""The Hidden Spring: the secret behind the Hidden Lake's waterfall, up the crack the falls pour out
of. A quiet place with a bench of its own, the stream that becomes the falls, and gems: one for the
long rope, one for when you can jump twice."""
from world.build import region, room, ground, roof

region('spring')

# ----------------------------------------------------------------------------------------------
# The Hidden Spring: in low on the right, from the passage at the top of the lake's crack, by a
# bench. A glowing pool along the floor, an island in it, and the stream running out of it towards
# the passage: the falls start here. Gems: on the island; one hanging over the pool, for a swing
# from the rock overhead; and one on a stack of slick stone by the far wall, too high for one jump
# (the feather's, at the end of the Sky Road).
# ----------------------------------------------------------------------------------------------

spring = room('hidden_spring', 'The Hidden Spring', 'grotto', 44, 24)
ground(spring, 1)
roof(spring, 20)
spring.door('R', 2)
spring.fill(8, 24, 1, 1, 'w')                   # the spring
spring.fill(15, 16, 1, 1, '#')                  # an island in it
spring.fill(27, 31, 12, 19, '#').fill(28, 30, 11, 11, '%')  # rock hanging from the roof, hook blocks under it
spring.fill(18, 22, 19, 19, '%')                # and hook blocks in the roof over the pool
spring.fill(1, 6, 2, 7, 'o')                    # a stack of slick stone by the far wall
spring.put(38, 2, 'S')
spring.put(34, 2, 'B')
spring.put(15, 2, '*')                          # on the island
spring.put(24, 9, '*')                         # over the pool
spring.put(3, 8, '*')                           # on the stack

spring.props((36, 2, 'glowshroom'), (41, 2, 'violet_crystals'), (26, 2, 'glowshroom'), (12, 2, 'lily'), (21, 2, 'lily'),
             (7, 2, 'bellflowers'), (10, 19, 'glow_roots'), (14, 19, 'stalactites'), (34, 19, 'glow_roots'),
             (39, 19, 'stalactites'), (5, 8, 'violet_crystals'), (24, 18, 'light_shaft'), (32, 2, 'runestone'))
